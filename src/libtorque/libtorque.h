#ifndef LIBTORQUE_LIBTORQUE
#define LIBTORQUE_LIBTORQUE

#ifdef __cplusplus
extern "C" {
#endif

#include <signal.h>

struct itimerspec;
struct libtorque_ctx;
struct libtorque_rxbuf;

// Errors can be converted to a string via libtorque_errstr().
typedef enum {
	LIBTORQUE_ERR_NONE = 0,
	LIBTORQUE_ERR_ASSERT,	// principal precondition/expectation failure
	LIBTORQUE_ERR_CPUDETECT,// error detecting available processor units
	LIBTORQUE_ERR_MEMDETECT,// error detecting available memories
	LIBTORQUE_ERR_AFFINITY, // error in the affinity subsystem
	LIBTORQUE_ERR_RESOURCE,	// insufficient memory or descriptors
	LIBTORQUE_ERR_INVAL,	// an invalid parameter was passed
	LIBTORQUE_ERR_SIGMASK,	// invalid signal mask upon entry to libtorque
	LIBTORQUE_ERR_UNAVAIL,	// functionality unavailable on this platform

	LIBTORQUE_ERR_MAX	// sentinel value, should never be seen
} libtorque_err;

// Properly mask signals used internally by libtorque. This ought be called
// prior to calling any libtorque functions, or creating any threads. If the
// parameter is not NULL, the current signal mask will be stored there.
libtorque_err libtorque_sigmask(sigset_t *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result));

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
//
// Upon successful return, EVTHREAD_TERM and EVTHREAD_INT will be blocked in
// the calling thread, even if they weren't before. To be totally safe,
// especially in programs with multiple threads outside of libtorque,
// libtorque_sigmask() ought be called prior to launching *any* threads. If the
// sigset_t parameter is not NULL, the old signal mask will be stored there.
struct libtorque_ctx *libtorque_init(libtorque_err *,sigset_t *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((malloc));

// Multiple threads may add event sources to a libtorque instance concurrently,
// so long as they are not adding the same event source (ie, the callers must
// be able to guarantee the signals, fds, whatever are not the same). The
// registration implementation is lock- and indeed wait-free.

// Callbacks get a triad: source, our callback state, and their own. Ours is
// just as opaque to them as theirs is to us. Continuations at their core, the
// unbuffered typedefs return void.
typedef void (*libtorquercb)(int,void *);
typedef void (*libtorquewcb)(int,void *);
typedef int (*libtorquebrcb)(int,struct libtorque_rxbuf *,void *);
typedef int (*libtorquebwcb)(int,struct libtorque_rxbuf *,void *);

// Invoke the callback upon receipt of any of the specified signals. The signal
// set may not contain EVTHREAD_TERM (usually SIGTERM), SIGKILL or SIGSTOP.
libtorque_err libtorque_addsignal(struct libtorque_ctx *,const sigset_t *,
					libtorquercb,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,2,3)));

// After a minimum time interval, invoke the callback as soon as possible.
libtorque_err libtorque_addtimer(struct libtorque_ctx *,
		const struct itimerspec *,libtorquercb,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,2,3)));

// Watch for events on the specified file descriptor, and invoke the callbacks.
// Employ libtorque's read buffering. A buffered read callback must return -1
// if the descriptor has been closed, and 0 otherwise.	
libtorque_err libtorque_addfd(struct libtorque_ctx *,int,libtorquebrcb,
					libtorquebwcb,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

// The same as libtorque_addfd, but manage buffering in the application,
// calling back immediately on all events (but not in more than one thread).
libtorque_err libtorque_addfd_unbuffered(struct libtorque_ctx *,int,
				libtorquercb,libtorquewcb,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

// The same as libtorque_addfd_unbuffered, but allow multiple threads to handle
// event readiness notifications concurrently. This is (currently) the
// preferred methodology for accept(2)ing sockets.
libtorque_err libtorque_addfd_concurrent(struct libtorque_ctx *,int,
				libtorquercb,libtorquewcb,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

// Watch for events on the specified path, and invoke the callback.
libtorque_err libtorque_addpath(struct libtorque_ctx *,const char *,
					libtorquercb,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,2,3)));

#ifndef LIBTORQUE_WITHOUT_SSL
#include <openssl/ssl.h>
#else
typedef void SSL_CTX;
#endif
// The SSL_CTX should be set up with the desired authentication parameters etc
// already (utility functions are provided to do this). If libtorque was not
// compiled with SSL support, returns LIBTORQUE_ERR_UNAVAIL.
libtorque_err libtorque_addssl(struct libtorque_ctx *,int,SSL_CTX *,
				libtorquebrcb,libtorquebwcb,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,3)));

// Performs a thread-local lookup of the current ctx. This must not be cached
// beyond the lifetime of the callback instance!
struct libtorque_ctx *libtorque_getcurctx(void)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result));

// Wait until the libtorque threads exit via pthread_join(), but don't send
// them the termination signal ourselves. Rather, we're waiting for either an
// intentional or freak exit of the threads. This version is slightly more
// robust than calling libtorque_stop() from an external control thread, in
// that the threads' exit will result in immediate program progression. With
// the other method, the threads could die, but your control threads is still
// running; it's in a sigwait() or something, not a pthread_join() (which would
// succeed immediately). The context, and all of its data, are destroyed.
libtorque_err libtorque_block(struct libtorque_ctx *)
	__attribute__ ((visibility("default")));

// Signal and reap the running threads, and free the context. No further calls
// may be made using this context following libtorque_stop().
libtorque_err libtorque_stop(struct libtorque_ctx *)
	__attribute__ ((visibility("default")));

// Translate the libtorque error code into a human-readable string.
const char *libtorque_errstr(libtorque_err)
	__attribute__ ((visibility("default")));

#ifdef __cplusplus
}
#endif

#endif
