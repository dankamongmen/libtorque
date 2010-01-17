#ifndef LIBTORQUE_EVENTS_PATH
#define LIBTORQUE_EVENTS_PATH

#ifdef __cplusplus
extern "C" {
#endif

#include <libtorque/libtorque.h>
#include <libtorque/events/sources.h>

struct evqueue;

int add_fswatch_to_evhandler(const struct evqueue *,const char *,libtorquercb,void *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull (1,2,3)));

#ifdef __cplusplus
}
#endif

#endif
