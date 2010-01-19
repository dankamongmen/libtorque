#ifndef LIBTORQUE_EVENTS_THREAD
#define LIBTORQUE_EVENTS_THREAD

#ifdef __cplusplus
extern "C" {
#endif

struct evectors;

#include <pthread.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/sources.h>

typedef struct evhandler {
	const evqueue *evq;		// can be (likely is) shared
	pthread_t nexttid;
	evectors evec;			// one for each thread
	evthreadstats stats;		// one for each thread
} evhandler;

evhandler *create_evhandler(const evqueue *,const stack_t *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,2)))
	__attribute__ ((malloc));

int create_efd(void)
	__attribute__ ((warn_unused_result));

void destroy_evhandler(const libtorque_ctx *,evhandler *)
	__attribute__ ((nonnull(1,2)));

void event_thread(libtorque_ctx *,evhandler *)
	__attribute__ ((nonnull(1,2)))
	__attribute__ ((noreturn));

// Used to set up common signal-related evtable sources during initialization
int initialize_common_sources(struct libtorque_ctx *,struct evtables *,const sigset_t *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,2)));

// Performs a thread-local lookup to get the libtorque context or evhandler. Do
// not cache across callback invocations!
evhandler *get_thread_evh(void)
	__attribute__ ((warn_unused_result));

libtorque_ctx *get_thread_ctx(void)
	__attribute__ ((warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif
