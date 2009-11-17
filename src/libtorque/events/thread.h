#ifndef LIBTORQUE_EVENTS_THREAD
#define LIBTORQUE_EVENTS_THREAD

#ifdef __cplusplus
extern "C" {
#endif

struct evtables;
struct evectors;

#include <libtorque/events/sources.h>

typedef struct evhandler {
	int efd;
	pthread_t nexttid;
	struct evhandler *nextev;
	pthread_mutex_t lock;
	struct evtables *evsources;
	struct evectors *externalvec;
	evthreadstats stats;
} evhandler;

evhandler *create_evhandler(struct evtables *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((malloc));

int destroy_evhandler(evhandler *);

void event_thread(evhandler *) __attribute__ ((noreturn));

#ifdef __cplusplus
}
#endif

#endif
