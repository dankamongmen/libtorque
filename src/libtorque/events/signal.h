#ifndef LIBTORQUE_EVENTS_SIGNALS
#define LIBTORQUE_EVENTS_SIGNALS

#ifdef __cplusplus
extern "C" {
#endif

#include <signal.h>
#include <libtorque/events/sources.h>

struct evhandler;
struct libtorque_ctx;

int add_signal_to_evhandler(struct libtorque_ctx *,const struct evqueue *,
				const sigset_t *,libtorquercb,void *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull (1,2,3,4)));

#ifdef __cplusplus
}
#endif

#endif
