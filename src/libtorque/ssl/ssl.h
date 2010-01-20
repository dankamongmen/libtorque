#ifndef LIBTORQUE_SSL_SSL
#define LIBTORQUE_SSL_SSL

#ifndef LIBTORQUE_WITHOUT_SSL

#ifdef __cplusplus
extern "C" {
#endif

struct ssl_cbstate;

#include <openssl/ssl.h>
#include <libtorque/events/sources.h>

// Call this only if OpenSSL hasn't already been properly initialized by some
// other code. If OpenSSL is elsewhere initialized, ensure the threads(3ssl)
// directives have been observed!
int libtorque_init_ssl(void) __attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result));

// Likewise, call this only if we called initialize_ssl().
int libtorque_stop_ssl(void) __attribute__ ((visibility("default")));

// Create a new SSL context, if one is not being provided to us. We only allow
// SSLv3/TLSv1, and require full certificate-based authentication, but allow
// specification of whether or not client authentication is required.
SSL_CTX *libtorque_ssl_ctx(const char *,const char *,const char *,unsigned)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((malloc));

SSL *new_ssl_conn(SSL_CTX *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((malloc));

struct ssl_cbstate *
create_ssl_cbstate(struct libtorque_ctx *,SSL_CTX *,void *,libtorquebrcb,libtorquebwcb)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,2)))
	__attribute__ ((malloc));

void free_ssl_cbstate(struct ssl_cbstate *);

void ssl_accept_rxfxn(int,void *) __attribute__ ((nonnull(2)));

int ssl_tx(int,struct ssl_cbstate *,const void *,int)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(2)));

#ifdef __cplusplus
}
#endif

#endif

#endif
