#ifndef LIBTORQUE_EVENTS_THREAD
#define LIBTORQUE_EVENTS_THREAD

#ifdef __cplusplus
extern "C" {
#endif

struct evtables;
struct evectors;

#include <libtorque/events/sysdep.h>
#include <libtorque/events/sources.h>

typedef struct evhandler {
	int efd;			// can be shared
	pthread_t nexttid;
	struct evhandler *nextev;	// makes a circular linked list
	struct evtables *evsources;	// lives in libtorque_ctx, shared
	evectors evec;			// one for each thread
	evthreadstats stats;		// one for each thread
} evhandler;

evhandler *create_evhandler(struct evtables *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)))
	__attribute__ ((malloc));

int destroy_evhandler(evhandler *);

void event_thread(libtorque_ctx *,evhandler *) __attribute__ ((noreturn));

#ifdef __cplusplus
}
#endif

#endif
