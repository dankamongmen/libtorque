#ifndef LIBTORQUE_WITHOUT_SSL
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
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

typedef struct rxbuffer {
	char *buffer;
	size_t bufleft;
} rxbuffer;

typedef struct ssl_cbstate {
	struct libtorque_ctx *ctx;
	SSL_CTX *sslctx;
	SSL *ssl;
	void *cbstate;
	libtorquecb rxfxn,txfxn;
	rxbuffer rxb;
} ssl_cbstate;

static inline char *
rxbuffer_buf(rxbuffer *rxb){
	return rxb->buffer;
}

static inline size_t
rxbuffer_avail(const rxbuffer *rxb){
	return rxb->bufleft;
}

#define RXBUFSIZE (16 * 1024)

static inline int
initialize_rxbuffer(rxbuffer *rxb){
	if( (rxb->buffer = malloc(RXBUFSIZE)) ){
		rxb->bufleft = RXBUFSIZE;
		return 0;
	}
	return -1;
}

static inline void
free_rxbuffer(rxbuffer *rxb){
	free(rxb->buffer);
}

struct ssl_cbstate *
create_ssl_cbstate(struct libtorque_ctx *ctx,SSL_CTX *sslctx,void *cbstate,
					libtorquecb rx,libtorquecb tx){
	ssl_cbstate *ret;

	if( (ret = malloc(sizeof(*ret))) ){
		if(initialize_rxbuffer(&ret->rxb) == 0){
			ret->ctx = ctx;
			ret->sslctx = sslctx;
			ret->cbstate = cbstate;
			ret->rxfxn = rx;
			ret->txfxn = tx;
			ret->ssl = NULL;
			return ret;
		}
		free(ret);
	}
	return NULL;
}

void free_ssl_cbstate(ssl_cbstate *sc){
	if(sc){
		SSL_free(sc->ssl);
		free_rxbuffer(&sc->rxb);
		free(sc);
	}
}

int libtorque_stop_ssl(void){
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

SSL_CTX *libtorque_ssl_ctx(const char *certfile,const char *keyfile,
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

int libtorque_init_ssl(void){
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

static int
ssl_rxfxn(int fd,void *cbs){
	ssl_cbstate *sc = cbs;
	int r,err;

	printf("%s\n",__func__);
	if(sc->rxfxn == NULL){
		return -1;
	}
	while((r = SSL_read(sc->ssl,rxbuffer_buf(&sc->rxb),rxbuffer_avail(&sc->rxb))) > 0){
		printf("read %d\n",r);
		if(sc->rxfxn(fd,sc->cbstate)){
			return -1;
		}
	}
	err = SSL_get_error(sc->ssl,r);
	printf("read %d %d\n",r,err);
	if(err == SSL_ERROR_WANT_WRITE){
		// need to change the callbacks! FIXME
	}else if(err == SSL_ERROR_WANT_READ){
		// just let it loop
	}else{
		free_ssl_cbstate(sc);
		close(fd);
		return -1;
	}
	return 0;
}

static int
ssl_txfxn(int fd,void *cbs){
	const ssl_cbstate *sc = cbs;

	printf("%s\n",__func__);
	if(sc->txfxn == NULL){
		return -1;
	}
	return sc->txfxn(fd,sc->cbstate);
}

static int
accept_contrxfxn(int fd,void *cbs){
	const ssl_cbstate *sc = cbs;
	int ret;

	if((ret = SSL_accept(sc->ssl)) == 1){
		// need to change the callbacks! FIXME
	}else{
		int err = SSL_get_error(sc->ssl,ret);

		if(err == SSL_ERROR_WANT_READ){
			// just let it loop
		}else if(err == SSL_ERROR_WANT_WRITE){
			// need to change the callbacks! FIXME
		}else{
			free_ssl_cbstate(cbs);
			close(fd);
			return -1;
		}
	}
	return 0;
}

static int
accept_conttxfxn(int fd,void *cbs){
	const ssl_cbstate *sc = cbs;
	int ret;

	if((ret = SSL_accept(sc->ssl)) == 1){
		// need to change the callbacks! FIXME
	}else{
		int err = SSL_get_error(sc->ssl,ret);

		if(err == SSL_ERROR_WANT_READ){
			// need to change the callbacks! FIXME
		}else if(err == SSL_ERROR_WANT_WRITE){
			// just let it loop
		}else{
			free_ssl_cbstate(cbs);
			close(fd);
			return -1;
		}
	}
	return 0;
}

static inline int
ssl_accept_internal(int sd,const ssl_cbstate *sc){
	ssl_cbstate *csc;
	int ret;

	csc = create_ssl_cbstate(sc->ctx,sc->sslctx,sc->cbstate,sc->rxfxn,sc->txfxn);
	if(csc == NULL){
		return -1;
	}
	if((csc->ssl = SSL_new(sc->sslctx)) == NULL){
		free_ssl_cbstate(csc);
		return -1;
	}
	if(SSL_set_fd(csc->ssl,sd) != 1){
		free_ssl_cbstate(csc);
		return -1;
	}
	if((ret = SSL_accept(csc->ssl)) == 1){
		libtorquecb rx = sc->rxfxn ? ssl_rxfxn : NULL;
		libtorquecb tx = sc->txfxn ? ssl_txfxn : NULL;

		if(libtorque_addfd(sc->ctx,sd,rx,tx,csc)){
			free_ssl_cbstate(csc);
			return -1;
		}
	}else{
		int err;

		err = SSL_get_error(csc->ssl,ret);
		if(err == SSL_ERROR_WANT_WRITE){
			if(libtorque_addfd(sc->ctx,sd,NULL,accept_conttxfxn,csc)){
				free_ssl_cbstate(csc);
				return -1;
			}
		}else if(err == SSL_ERROR_WANT_READ){
			if(libtorque_addfd(sc->ctx,sd,accept_contrxfxn,NULL,csc)){
				free_ssl_cbstate(csc);
				return -1;
			}
		}else{
			free_ssl_cbstate(csc);
			return -1;
		}
	}
	return 0;
}

int ssl_accept_rxfxn(int fd,void *cbs){
	const ssl_cbstate *sc = cbs;
	struct sockaddr_in sina;
	socklen_t slen;
	int sd;

	do{
		slen = sizeof(sina);
		while((sd = accept(fd,(struct sockaddr *)&sina,&slen)) < 0){
			if(errno != EINTR){ // loop on EINTR
				return 0;
			}
		}
		if(ssl_accept_internal(sd,sc)){
			close(sd);
		}
	}while(1);
}
#endif
