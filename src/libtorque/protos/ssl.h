#ifndef LIBTORQUE_PROTOS_SSL
#define LIBTORQUE_PROTOS_SSL

#ifndef LIBTORQUE_WITHOUT_SSL

#ifdef __cplusplus
extern "C" {
#endif

struct ssl_cbstate;

#include <openssl/ssl.h>
#include <libtorque/events/sources.h>

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

#ifdef __cplusplus
}
#endif

#endif

#endif
