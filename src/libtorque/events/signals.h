#ifndef LIBTORQUE_EVENTS_SIGNALS
#define LIBTORQUE_EVENTS_SIGNALS

#ifdef __cplusplus
extern "C" {
#endif

#include <libtorque/events/sources.h>

struct evhandler;

int add_signal_to_evhandler(struct evhandler *,int,evcbfxn,void *)
	__attribute__ ((nonnull (1,3)));

#ifdef __cplusplus
}
#endif

#endif
