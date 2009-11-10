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
	__attribute__ ((visibility("default")));

// Returning anything other than 0 will see the descriptor closed, and removed
// from the evhandler's notification queue.
// FIXME maybe ought be using a uintptr_t instead of void *?
typedef void (*libtorque_evcbfxn)(unsigned,void *);
struct libtorque_evsource;

// Add an event source. It won't be acted upon until a successful call to
// libtorque_spawn(). Event sources may be added after calling
// libtorque_spawn(), but not after calling libtorque_stop().
int libtorque_addevent(const struct libtorque_evsource *)
	__attribute__ ((visibility("default")));

// Spawn event-handling threads. They'll run until a call is made to
// libtorque_reap(), which may be performed from within a callback function.
int libtorque_spawn(struct libtorque_ctx *)
	__attribute__ ((visibility("default")));

// Reap event-handling threads. May be called externally, or by a callback
// function. More than one thread may safely call libtorque_reap() at once.
int libtorque_reap(struct libtorque_ctx *)
	__attribute__ ((visibility("default")));

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
