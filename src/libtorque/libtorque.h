#ifndef LIBTORQUE_LIBTORQUE
#define LIBTORQUE_LIBTORQUE

#ifdef __cplusplus
extern "C" {
#endif

struct SSL_CTX;
struct libtorque_ctx;

// Initialize the library, returning 0 on success. No libtorque functions may
// be called before a successful call to libtorque_init(). libtorque_init() may
// not be called again until libtorque_stop() has been called. Implicitly, only
// one thread may call libtorque_init(). The flags argument is any combination
// of the following flags:
//
// LIBTORQUE_INIT_NOSSLINIT: Don't perform OpenSSL library initialization,
//  either because OpenSSL won't be used or it's been initialized elsewhere. In
//  particular, this won't initialize the threads(3ssl) code.
#define LIBTORQUE_INIT_NOSSLINIT	0x00000001
struct libtorque_ctx *libtorque_init(unsigned)
	__attribute__ ((visibility("default")))
	__attribute__ ((malloc));

// Returning anything other than 0 will see the descriptor closed, and removed
// from the evhandler's notification queue.
// FIXME maybe ought be using a uintptr_t instead of void *?
typedef int (*libtorque_evcbfxn)(int,void *);

// Handle the specified signal (we don't use a sigset_t because FreeBSD's
// kqueue doesn't support it; we could wrap this, though FIXME).
int libtorque_addsignal(struct libtorque_ctx *,int,libtorque_evcbfxn,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((nonnull(1,3)));

// Handle the specified file descriptor.
int libtorque_addfd(struct libtorque_ctx *,int,libtorque_evcbfxn,
				libtorque_evcbfxn,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((nonnull(1)));

// The SSL_CTX should be set up with the desired authentication parameters etc
// already (utility functions are provided to do this).
int libtorque_addssl(struct libtorque_ctx *,int,struct SSL_CTX *,
			libtorque_evcbfxn,libtorque_evcbfxn,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((nonnull(1,3)));

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
