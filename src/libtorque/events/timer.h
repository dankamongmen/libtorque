#ifndef LIBTORQUE_EVENTS_TIMER
#define LIBTORQUE_EVENTS_TIMER

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <libtorque/internal.h>

torque_err add_timer_to_evhandler(struct libtorque_ctx *,
			const struct evqueue *,const struct itimerspec *,
			libtorquetimecb,void *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull (1,2,3,4)));

typedef struct timerfd_marshal {
	libtorquetimecb tfxn;
	void *cbstate;
} timerfd_marshal;

static inline void
timer_curry(void *state){
	timerfd_marshal *marsh = state;

	marsh->tfxn(marsh->cbstate);
	free(marsh);
}

#ifdef __cplusplus
}
#endif

#endif
