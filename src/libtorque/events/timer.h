#ifndef LIBTORQUE_EVENTS_TIMER
#define LIBTORQUE_EVENTS_TIMER

#ifdef __cplusplus
extern "C" {
#endif

#include <libtorque/internal.h>

libtorque_err add_timer_to_evhandler(struct libtorque_ctx *,
			const struct evqueue *,const struct itimerspec *,
			libtorquetimecb,void *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull (1,2,3,4)));

void timerfd_passthru(int __attribute__ ((unused)),void *);

#ifdef __cplusplus
}
#endif

#endif
