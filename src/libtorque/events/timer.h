#ifndef LIBTORQUE_EVENTS_TIMER
#define LIBTORQUE_EVENTS_TIMER

#ifdef __cplusplus
extern "C" {
#endif

#include <libtorque/events/sources.h>

struct evqueue;
struct libtorque_ctx;

int add_timer_to_evhandler(struct libtorque_ctx *,const struct evqueue *,
			const struct itimerspec *,libtorquetimecb,void *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull (1,2,3,4)));

#ifdef __cplusplus
}
#endif

#endif
