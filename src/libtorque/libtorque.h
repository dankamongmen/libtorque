#ifndef LIBTORQUE_LIBTORQUE
#define LIBTORQUE_LIBTORQUE

#ifdef __cplusplus
extern "C" {
#endif

struct libtorque_ctx;

// Initialize the library, returning 0 on success. No libtorque functions may
// be called before a successful call to libtorque_init(). libtorque_init() may
// not be called again until libtorque_stop() has been called. Implicitly, only
// one thread may call libtorque_init().
struct libtorque_ctx *libtorque_init(void)
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
