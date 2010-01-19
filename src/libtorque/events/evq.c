#include <unistd.h>
#include <libtorque/events/evq.h>
#include <libtorque/events/thread.h>

int destroy_evqueue(evqueue *evq){
	int ret = 0;

	ret |= close(evq->efd);
	evq->efd = -1;
	return ret;
}

#ifdef LIBTORQUE_LINUX
// fd is the common signalfd
static inline int
add_commonfds_to_evhandler(int fd,evqueue *evq){
	struct epoll_ctl_data ecd;
	struct epoll_event ee;
	struct kevent k;

	if(fd < 0){
		return -1;
	}
	memset(&ee.data,0,sizeof(ee.data));
	ee.data.fd = fd;
	// We automatically wait for EPOLLERR/EPOLLHUP; according to
	// epoll_ctl(2), "it is not necessary to add set [these] in ->events"
	// We do NOT use edge-triggered polling for this internal handler, for
	// obscure technical reasons (a thread which bails can't easily empty
	// the pending signal queue, at least not without reading everything
	// else, putting it on an event queue, and then swapping the exit back
	// in from some other queue). So no EPOLLET.
	ee.events = EPOLLIN;
	k.events = &ee;
	k.ctldata = &ecd;
	ecd.op = EPOLL_CTL_ADD;
	if(add_evector_kevents(evq,&k,1)){
		return -1;
	}
	return 0;
}
#endif

static inline int
add_evqueue_baseevents(const libtorque_ctx *ctx,evqueue *e){
#ifdef LIBTORQUE_FREEBSD
	if(add_signal_to_evhandler(ctx,e,&s,rxcommonsignal,NULL)){
		return -1;
	}
	return 0;
#elif defined(LIBTORQUE_LINUX_SIGNALFD)
	return add_commonfds_to_evhandler(ctx->eventtables.common_signalfd,e);
#elif defined(LIBTORQUE_LINUX)
	if(!ctx || !e){ // FIXME just to silence -Wunused warnings, blargh
		return -1;
	}
	return 0;
#endif
}

int init_evqueue(libtorque_ctx *ctx,evqueue *e){
	if((e->efd = create_efd()) < 0){
		return -1;
	}
	if(add_evqueue_baseevents(ctx,e)){
		close(e->efd);
		return -1;
	}
	return 0;
}
