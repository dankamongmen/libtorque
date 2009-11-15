#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <libtorque/ssl/ssl.h>
#include <libtorque/schedule.h>

static unsigned numlocks;
static pthread_mutex_t *openssl_locks;

struct CRYPTO_dynlock_value {
	pthread_mutex_t mutex; // FIXME use a read-write lock
};

typedef struct ssl_accept_cbstate {
	SSL *ssl;
	void *cbstate;
	libtorque_evcbfxn rxfxn,txfxn;
} ssl_accept_cbstate;

struct ssl_accept_cbstate *
create_ssl_accept_cbstate(SSL_CTX *ctx,void *cbstate,libtorque_evcbfxn rx,
						libtorque_evcbfxn tx){
	ssl_accept_cbstate *ret;

	if( (ret = malloc(sizeof(*ret))) ){
		if((ret->ssl = new_ssl_conn(ctx)) == NULL){
			free(ret);
			return NULL;
		}
		ret->cbstate = cbstate;
		ret->rxfxn = rx;
		ret->txfxn = tx;
	}
	return ret;
}

void free_ssl_accept_cbstate(ssl_accept_cbstate *sac){
	if(sac){
		SSL_free(sac->ssl);
		free(sac);
	}
}

int stop_ssl(void){
	int ret = 0;
	unsigned z;

	RAND_cleanup();
	CRYPTO_set_locking_callback(NULL);
	CRYPTO_set_id_callback(NULL);
	for(z = 0 ; z < numlocks ; ++z){
		if(pthread_mutex_destroy(&openssl_locks[z])){
			ret = -1;
		}
	}
	numlocks = 0;
	free(openssl_locks);
	openssl_locks = NULL;
	return ret;
}

// See threads(3SSL)
static void
openssl_lock_callback(int mode,int n,const char *file __attribute__ ((unused)),
				int line __attribute__ ((unused))){
	if(mode & CRYPTO_LOCK){
		pthread_mutex_lock(&openssl_locks[n]);
	}else{
		pthread_mutex_unlock(&openssl_locks[n]);
	}
}

static unsigned long
openssl_id_callback(void){
	// FIXME pthread_self() doesn't return an integer type on (at least)
	// FreeBSD...argh :(
	return pthread_self_getnumeric();
}

static struct CRYPTO_dynlock_value *
openssl_dyncreate_callback(const char *file __attribute__ ((unused)),
				int line __attribute__ ((unused))){
	struct CRYPTO_dynlock_value *ret;

	if( (ret = malloc(sizeof(*ret))) ){
		if(pthread_mutex_init(&ret->mutex,NULL)){
			free(ret);
			return NULL;
		}
	}
	return ret;
}

static void
openssl_dynlock_callback(int mode,struct CRYPTO_dynlock_value *l,
				const char *file __attribute__ ((unused)),
				int line __attribute__ ((unused))){
	if(mode & CRYPTO_LOCK){
		pthread_mutex_lock(&l->mutex);
	}else{
		pthread_mutex_unlock(&l->mutex);
	}
}

static void
openssl_dyndestroy_callback(struct CRYPTO_dynlock_value *l,
				const char *file __attribute__ ((unused)),
				int line __attribute__ ((unused))){
	pthread_mutex_destroy(&l->mutex);
	free(l);
}

static int
openssl_verify_callback(int preverify_ok,X509_STORE_CTX *xctx __attribute__ ((unused))){
	if(preverify_ok){ // otherwise, OpenSSL's already blackballed the conn
		// FIXME check CRL etc
	}
	return preverify_ok;
}

SSL_CTX *new_ssl_ctx(const char *certfile,const char *keyfile,
			const char *cafile,unsigned cliver){
	SSL_CTX *ret;

	if((ret = SSL_CTX_new(SSLv3_method())) == NULL){
		return NULL;
	}
	// The CA's we trust. We must still ensure the certificate chain is
	// semantically valid, not just syntactically valid! This is done via
	// checking X509 properties and a CRL in openssl_verify_callback().
	if(cafile){
		if(SSL_CTX_load_verify_locations(ret,cafile,NULL) != 1){
			SSL_CTX_free(ret);
			return NULL;;
		}
	}
	if(cliver){
		SSL_CTX_set_verify(ret,SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
					openssl_verify_callback);
	}else{
		SSL_CTX_set_verify(ret,SSL_VERIFY_NONE,NULL);
	}
	if(!(keyfile || certfile)){
		return ret;
	}
	if(SSL_CTX_use_PrivateKey_file(ret,keyfile,SSL_FILETYPE_PEM) == 1){
		if(SSL_CTX_use_certificate_chain_file(ret,certfile) == 1){
			return ret;
		}
	}else if(SSL_CTX_use_PrivateKey_file(ret,keyfile,SSL_FILETYPE_ASN1) == 1){
		if(SSL_CTX_use_certificate_chain_file(ret,certfile) == 1){
			return ret;
		}
	}
	SSL_CTX_free(ret);
	return NULL;
}

int init_ssl(void){
	int nlocks;
	unsigned z;

	if(SSL_library_init() != 1){
		return -1;
	}
	SSL_load_error_strings();
	// OpenSSL transparently seeds the entropy pool on systems supporting a
	// /dev/urandom device. Otherwise, one needs seed it using truly random
	// data from some source (EGD, preserved file, etc). We currently just
	// hope for a /dev/urandom, but at least verify the pool's OK FIXME.
	if(RAND_status() != 1){
		return -1;
	}
	if((nlocks = CRYPTO_num_locks()) < 0){
		RAND_cleanup();
		return -1;
	}else if(nlocks == 0){ // assume no need for threading
		return 0;
	}
	numlocks = nlocks;
	if((openssl_locks = malloc(sizeof(*openssl_locks) * numlocks)) == NULL){
		RAND_cleanup();
		return -1;
	}
	for(z = 0 ; z < numlocks ; ++z){
		if(pthread_mutex_init(&openssl_locks[z],NULL)){
			while(z--){
				pthread_mutex_destroy(&openssl_locks[z]);
			}
			free(openssl_locks);
			openssl_locks = NULL;
			numlocks = 0;
			RAND_cleanup();
			return -1;
		}
	}
	CRYPTO_set_locking_callback(openssl_lock_callback);
	CRYPTO_set_id_callback(openssl_id_callback);
	CRYPTO_set_dynlock_create_callback(openssl_dyncreate_callback);
	CRYPTO_set_dynlock_lock_callback(openssl_dynlock_callback);
	CRYPTO_set_dynlock_destroy_callback(openssl_dyndestroy_callback);
	return 0;
}

SSL *new_ssl_conn(SSL_CTX *ctx){
	return SSL_new(ctx);
}

int ssl_accept_rxfxn(int fd,void *cbs){
	const ssl_accept_cbstate *sac = cbs;
	int ret,err;

	if((ret = SSL_accept(sac->ssl)) == 0){
		return sac->rxfxn(fd,sac->cbstate);
	}
	err = SSL_get_error(sac->ssl,ret);
	printf("SSL error %d %d\n",ret,err);
	if(err == SSL_ERROR_WANT_WRITE){
		// FIXME
	}else if(err == SSL_ERROR_WANT_READ){
		return 0; // just wait for next event
	}
	return -1;
}
