#ifndef LIBTORQUE_SSL_SSL
#define LIBTORQUE_SSL_SSL

#ifdef __cplusplus
extern "C" {
#endif

#include <openssl/ssl.h>

struct SSL;
struct SSL_CTX;

// Call this only if OpenSSL hasn't already been properly initialized by some
// other code. If OpenSSL is elsewhere initialized, ensure the threads(3ssl)
// directives have been observed!
int init_ssl(void);

// Likewise, call this only if we called initialize_ssl().
int stop_ssl(void);

// Create a new SSL context, if one is not being provided to us. We only allow
// SSLv3/TLSv1, and require full certificate-based authentication, but allow
// specification of whether or not client authentication is required.
SSL_CTX *new_ssl_ctx(const char *,const char *,const char *,unsigned);

SSL *new_ssl_conn(SSL_CTX *);

#ifdef __cplusplus
}
#endif

#endif
