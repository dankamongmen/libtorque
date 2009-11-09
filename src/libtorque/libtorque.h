#ifndef LIBTORQUE_LIBTORQUE
#define LIBTORQUE_LIBTORQUE

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the library, returning 0 on success. No libtorque functions may
// be called before a successful call to libtorque_init(). libtorque_init() may
// not be called again until libtorque_stop() has been called. Implicitly, only
// one thread may call libtorque_init().
int libtorque_init(void) __attribute__ ((visibility("default")));

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

// Spawn event-handling threads. Currently, this function returns immediately
// following a per-CPU spawning. Future designs might see the calling thread
// become the event-handling thread on its CPU, only returning when the event
// handling terminates via internal call to libtorque_reap().
int libtorque_spawn(void) __attribute__ ((visibility("default")));

// Reap event-handling threads.
int libtorque_reap(void) __attribute__ ((visibility("default")));

// Reset the library, destroying all associated threads and state and returning
// 0 on success. No libtorque functions, save libtorque_init(), may be called
// following a call to libtorque_stop() (whether it is successful or not).
// libtorque_init() must not be called from multiple threads, or while another
// thread is calling a libtorque function. A successful return guarantees that
// no further callbacks will be issued.
int libtorque_stop(void) __attribute__ ((visibility("default")));

#ifdef __cplusplus
}
#endif

#endif
