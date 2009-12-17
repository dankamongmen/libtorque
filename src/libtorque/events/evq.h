#ifndef LIBTORQUE_EVENTS_EVQ
#define LIBTORQUE_EVENTS_EVQ

#ifdef __cplusplus
extern "C" {
#endif

struct evqueue;

#include <libtorque/internal.h>
#include <libtorque/events/sysdep.h>

int init_evqueue(libtorque_ctx *,struct evqueue *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

int ref_evqueue(struct evqueue *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

int destroy_evqueue(struct evqueue *);

static inline int
flush_evqueue_changes(const evqueue *evq,evectors *ev){
	int ret = 0;

	if(ev->changesqueued){
#ifdef LIBTORQUE_LINUX
		ret = Kevent(evq->efd,&ev->changev,ev->changesqueued,NULL,0);
#else
		ret = Kevent(evq->efd,ev->changev,ev->changesqueued,NULL,0);
#endif
		ev->changesqueued = 0;
	}
	return ret;
}

#ifdef __cplusplus
}
#endif

#endif
