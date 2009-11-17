#ifndef LIBTORQUE_EVENTS_THREAD
#define LIBTORQUE_EVENTS_THREAD

#ifdef __cplusplus
extern "C" {
#endif

struct evtables;

#include <libtorque/events/sources.h>

evhandler *create_evhandler(struct evtables *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((malloc));

int destroy_evhandler(evhandler *);

void event_thread(evhandler *) __attribute__ ((noreturn));

#ifdef __cplusplus
}
#endif

#endif
