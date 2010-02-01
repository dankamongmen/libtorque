#ifndef LIBTORQUE_LIBTORQUE
#define LIBTORQUE_LIBTORQUE

#ifdef __cplusplus
extern "C" {
#endif

#include <signal.h>

struct itimerspec;
struct torque_ctx;
struct libtorque_rxbuf;

// Errors can be converted to a string via torque_errstr().
typedef enum {
	TORQUE_ERR_NONE = 0,
	TORQUE_ERR_ASSERT,      // principal precondition/expectation failure
	TORQUE_ERR_CPUDETECT,   // error detecting available processor units
	TORQUE_ERR_MEMDETECT,   // error detecting available memories
	TORQUE_ERR_AFFINITY,    // error in the affinity subsystem
	TORQUE_ERR_RESOURCE,    // insufficient memory or descriptors
	TORQUE_ERR_INVAL,       // an invalid parameter was passed
	TORQUE_ERR_UNAVAIL,     // functionality unavailable on this platform

	TORQUE_ERR_MAX          // sentinel value, should never be seen
} torque_err;

// Properly mask signals used internally by libtorque. This ought be called
// prior to creating any threads. If the parameter is not NULL, the current
// signal mask will be stored there.
torque_err libtorque_sigmask(sigset_t *)
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
// If libtorque_block() or libtorque_stop() are to be used, it is imperative
// that all other threads have libtorque's internally-used signals masked. This
// is best accomplished by calling libtorque_sigmask() early in the program.
struct torque_ctx *libtorque_init(torque_err *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)))
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
typedef void (*libtorquetimecb)(void *);
typedef int (*libtorquebrcb)(int,struct libtorque_rxbuf *,void *);
typedef int (*libtorquebwcb)(int,struct libtorque_rxbuf *,void *);

// Invoke the callback upon receipt of any of the specified signals. The signal
// set may not contain EVTHREAD_TERM (usually SIGTERM), SIGKILL or SIGSTOP.
torque_err libtorque_addsignal(struct torque_ctx *,const sigset_t *,
					libtorquercb,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,2,3)));

// After a minimum time interval, invoke the callback as soon as possible.
torque_err libtorque_addtimer(struct torque_ctx *,
		const struct itimerspec *,libtorquetimecb,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,2,3)));

// Watch for events on the specified file descriptor, and invoke the callbacks.
// Employ libtorque's read buffering. A buffered read callback must return -1
// if the descriptor has been closed, and 0 otherwise.	
torque_err libtorque_addfd(struct torque_ctx *,int,libtorquebrcb,
					libtorquebwcb,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

// The same as libtorque_addfd, but manage buffering in the application,
// calling back immediately on all events (but not in more than one thread).
torque_err libtorque_addfd_unbuffered(struct torque_ctx *,int,
				libtorquercb,libtorquewcb,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

// The same as libtorque_addfd_unbuffered, but allow multiple threads to handle
// event readiness notifications concurrently. This is (currently) the
// preferred methodology for accept(2)ing sockets.
torque_err libtorque_addfd_concurrent(struct torque_ctx *,int,
				libtorquercb,libtorquewcb,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

// Watch for events on the specified path, and invoke the callback.
torque_err libtorque_addpath(struct torque_ctx *,const char *,
					libtorquercb,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,2,3)));

#ifndef LIBTORQUE_WITHOUT_SSL
#include <openssl/ssl.h>
#else
typedef void SSL_CTX;
#endif

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

// The SSL_CTX should be set up with the desired authentication parameters etc
// already (utility functions are provided to do this). If libtorque was not
// compiled with SSL support, returns torque_err_UNAVAIL.
torque_err libtorque_addssl(struct torque_ctx *,int,SSL_CTX *,
				libtorquebrcb,libtorquebwcb,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,3)));

struct ssl_cbstate;

int ssl_tx(int,struct ssl_cbstate *,const void *,int)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(2)));

#ifndef LIBTORQUE_WITHOUT_ADNS
#include <adns.h>
typedef adns_answer libtorque_dnsret;
#else
typedef void libtorque_dnsret;
#endif
typedef void (*libtorquednscb)(const libtorque_dnsret *,void *);
// FIXME probably ought take adns_rrtype and adns_queryflags as well...
torque_err libtorque_addlookup_dns(struct torque_ctx *,const char *,
						libtorquednscb,void *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,2,3)));

// Performs a thread-local lookup of the current ctx. This must not be cached
// beyond the lifetime of the callback instance!
struct torque_ctx *libtorque_getcurctx(void)
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
torque_err libtorque_block(struct torque_ctx *)
	__attribute__ ((visibility("default")));

// Signal and reap the running threads, and free the context. No further calls
// may be made using this context following libtorque_stop().
torque_err libtorque_stop(struct torque_ctx *)
	__attribute__ ((visibility("default")));

// Translate the libtorque error code into a human-readable string.
const char *torque_errstr(torque_err)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif
