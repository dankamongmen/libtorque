#ifndef LIBTORQUE_LIBTORQUE
#define LIBTORQUE_LIBTORQUE

#ifdef __cplusplus
extern "C" {
#endif

#include <signal.h>

struct libtorque_ctx;

// Initialize the library, returning 0 on success. No libtorque functions may
// be called before a successful call to libtorque_init(). libtorque_init() may
// not be called again until libtorque_stop() has been called. Implicitly, only
// one thread may call libtorque_init().
struct libtorque_ctx *libtorque_init(void)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((malloc));

typedef struct torquercbstate {
	void *rxbuf;
	void *cbstate;
} torquercbstate;

// Returning anything other than 0 will see the descriptor closed, and removed
// from the evhandler's notification queue.
typedef int (*libtorquercb)(int,torquercbstate *);
typedef int (*libtorquewcb)(int,void *);

// Handle the specified signals.
int libtorque_addsignal(struct libtorque_ctx *,const sigset_t *,libtorquercb,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,2,3)));

// Handle the specified file descriptor.
int libtorque_addfd(struct libtorque_ctx *,int,libtorquercb,libtorquewcb,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

#ifndef LIBTORQUE_WITHOUT_SSL
#include <openssl/ssl.h>
// The SSL_CTX should be set up with the desired authentication parameters etc
// already (utility functions are provided to do this).
int libtorque_addssl(struct libtorque_ctx *,int,SSL_CTX *,libtorquercb,
						libtorquewcb,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,3)));
#endif

// Reset the library, destroying all associated threads and state and returning
// 0 on success. No libtorque functions, save libtorque_init(), may be called
// following a call to libtorque_stop() (whether it is successful or not).
// libtorque_init() must not be called from multiple threads, or while another
// thread is calling a libtorque function. A successful return guarantees that
// no further callbacks will be issued.
int libtorque_stop(struct libtorque_ctx *)
	__attribute__ ((visibility("default")));

#ifdef __cplusplus
}
#endif

#endif
