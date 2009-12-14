#ifndef LIBTORQUE_EVENTS_THREAD
#define LIBTORQUE_EVENTS_THREAD

#ifdef __cplusplus
extern "C" {
#endif

struct evtables;
struct evectors;

#include <pthread.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/sources.h>

typedef struct evqueue {
	int efd;
	unsigned refcount;
	pthread_mutex_t lock;
} evqueue;

typedef struct evhandler {
	evqueue *evq;			// can be (likely is) shared
	pthread_t nexttid;
	struct evhandler *nextev;	// makes a circular linked list
	struct evtables *evsources;	// lives in libtorque_ctx, shared
	evectors evec;			// one for each thread
	evthreadstats stats;		// one for each thread
} evhandler;

evhandler *create_evhandler(struct evtables *,evqueue *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,2)))
	__attribute__ ((malloc));

int create_efd(void)
	__attribute__ ((warn_unused_result));

int destroy_evhandler(evhandler *);

int destroy_evqueue(evqueue *);

void event_thread(evhandler *)
	__attribute__ ((nonnull(1)))
	__attribute__ ((noreturn));

#ifdef __cplusplus
}
#endif

#endif
