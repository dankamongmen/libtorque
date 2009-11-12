#ifndef LIBTORQUE_EVENTS_FDS
#define LIBTORQUE_EVENTS_FDS

#ifdef __cplusplus
extern "C" {
#endif

#include <libtorque/events/sources.h>

struct evhandler;

int add_fd_to_evhandler(struct evhandler *,int,evcbfxn,evcbfxn,void *)
	__attribute__ ((nonnull (1)));

#ifdef __cplusplus
}
#endif

#endif
