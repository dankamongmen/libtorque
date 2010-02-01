#ifndef LIBTORQUE_EVENTS_SIGNALS
#define LIBTORQUE_EVENTS_SIGNALS

#ifdef __cplusplus
extern "C" {
#endif

#include <signal.h>
#include <libtorque/internal.h>
#include <libtorque/events/sources.h>

torque_err add_signal_to_evhandler(struct torque_ctx *,
				const struct evqueue *,const sigset_t *,
				libtorquercb,void *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull (1,2,3,4)));

int init_signal_handlers(void)
	__attribute__ ((warn_unused_result));

void signal_demultiplexer(int);

#ifdef __cplusplus
}
#endif

#endif
