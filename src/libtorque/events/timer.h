#ifndef LIBTORQUE_EVENTS_TIMER
#define LIBTORQUE_EVENTS_TIMER

#ifdef __cplusplus
extern "C" {
#endif

#include <libtorque/events/sources.h>

struct evhandler;

int add_timer_to_evhandler(struct evhandler *,const struct itimerspec *,libtorquercb,void *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull (1,2,3)));

#ifdef __cplusplus
}
#endif

#endif
