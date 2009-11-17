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
// Create a new libtorque context on the current cpuset. The best performance
// on the widest set of loads and requirements is achieved by using one
// libtorque instance on as many uncontested processing elements as possible,
// but various reasons exist for running multiple instances:
//
// - Differentiated services (QoS): Sets of connections could be guaranteed
//    sets of processing elements
// - Combination of multiple (possibly closed-source) libtorque clients
// - libtorque's scheduling, especially initially, is likely to be subobtimal
//    for some architecture + code combinations; certain situations might be
//    improved using such a technique (I've not got examples, and such cases
//    ought be controllable via hints/feedbacks/heuristics).
// - Trading performance for fault-tolerance (putting less-essential events in
//    their own libtorque "ring" in case of lock ups in buggy callbacks, using
//    distinct alternate signal stacks...).
// - Trading performance for priority separation (see sched_setscheduler()).
//
// If multiple instances are used, the highest performance will generally be
// had running them all with as large a cpuset as possible (ie, overlapping
// cpusets are no problem, and usually desirable). Again, make sure you really
// want to be using multiple instances.
struct libtorque_ctx *libtorque_init(void)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((malloc));

typedef struct torquercbstate {
	void *rxbuf;
	void *cbstate;
} torquercbstate;

// Multiple threads may add event sources to a libtorque instance concurrently,
// so long as they are not adding the same event source (ie, the callers must
// be able to guarantee the signals, fds, whatever are not the same). The
// registration implementation is lock- and indeed wait-free.

// Read callbacks get a dyad: their opaque callback state, and our own.
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

// Wait until the libtorque threads exit via pthread_join(), but don't send
// them the termination signal ourselves. Rather, we're waiting for either an
// intentional or freak exit of the threads. This version is slightly more
// robust than calling libtorque_stop() from an external control thread, in
// that the threads' exit will result in immediate program progression. With
// the other method, the threads could die, but your control threads is still
// running; it's in a sigwait() or something, not a pthread_join() (which would
// succeed immediately). The context, and all of its data, are destroyed.
int libtorque_block(struct libtorque_ctx *)
	__attribute__ ((visibility("default")));

// Signal and reap the running threads, and free the context.
int libtorque_stop(struct libtorque_ctx *)
	__attribute__ ((visibility("default")));

#ifdef __cplusplus
}
#endif

#endif
