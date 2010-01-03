#ifndef LIBTORQUE_EVENTS_FDS
#define LIBTORQUE_EVENTS_FDS

#ifdef __cplusplus
extern "C" {
#endif

#include <libtorque/libtorque.h>
#include <libtorque/events/sources.h>

struct evectors;
struct evhandler;

int add_fd_to_evhandler(struct evhandler *,int,libtorquercb,
				libtorquewcb,libtorque_cbctx *,void *,int)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull (1)));

#ifdef __cplusplus
}
#endif

#endif
