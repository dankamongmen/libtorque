#include <unistd.h>
#include <libtorque/dns/dns.h>
#include <libtorque/events/evq.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/signal.h>

int destroy_evqueue(evqueue *evq){
	int ret = 0;

	ret |= close(evq->efd);
	evq->efd = -1;
	libtorque_dns_shutdown(&evq->dnsctx);
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
	if(Kevent(evq->efd,&k,1,NULL,0)){
		return -1;
	}
	return 0;
}
#endif

static inline int
add_evqueue_baseevents(libtorque_ctx *ctx,evqueue *e){
	if(load_dns_fds(ctx,&e->dnsctx,e)){
		return -1;
	}
#ifdef LIBTORQUE_FREEBSD
	{
		sigset_t s;

		if(sigemptyset(&s) || sigaddset(&s,EVTHREAD_TERM) || sigaddset(&s,EVTHREAD_INT)){
			return -1;
		}
		if(add_signal_to_evhandler(ctx,e,&s,rxcommonsignal,NULL)){
			return -1;
		}
	}
	return 0;
#elif defined(LIBTORQUE_LINUX_SIGNALFD)
	return add_commonfds_to_evhandler(ctx->eventtables.common_signalfd,e);
#else
	return 0;
#endif
}

int init_evqueue(libtorque_ctx *ctx,evqueue *e){
	if(libtorque_dns_init(&e->dnsctx)){
		return -1;
	}
	if((e->efd = create_efd()) < 0){
		libtorque_dns_shutdown(&e->dnsctx);
		return -1;
	}
	if(add_evqueue_baseevents(ctx,e)){
		close(e->efd);
		libtorque_dns_shutdown(&e->dnsctx);
		return -1;
	}
	return 0;
}
