#ifndef LIBTORQUE_EVENTS_SIGNALS
#define LIBTORQUE_EVENTS_SIGNALS

#ifdef __cplusplus
extern "C" {
#endif

#include <signal.h>
#include <libtorque/events/sources.h>

struct evhandler;

int add_signal_to_evhandler(struct evhandler *,const sigset_t *,libtorquercb,void *)
	__attribute__ ((nonnull (1,2,3)));

#ifdef __cplusplus
}
#endif

#endif
