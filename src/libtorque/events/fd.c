#include <pthread.h>
#include <libtorque/events/fd.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/sources.h>

static inline int
add_fd_event(const evqueue *evq,int fd,libtorquercb rfxn,libtorquewcb tfxn,int eflags){
#ifdef TORQUE_LINUX
	struct epoll_ctl_data ecd;
	struct epoll_event ee;
	struct kevent k;

	if(eflags & ~(EPOLLONESHOT)){ // enforce allowed eflags
		return -1;
	}
	memset(&ee.data,0,sizeof(ee.data));
	k.events = &ee;
	ee.data.fd = fd;
	k.ctldata = &ecd;
	ecd.op = EPOLL_CTL_ADD;
	// We automatically wait for EPOLLERR/EPOLLHUP; according to
	// epoll_ctl(2), "it is not necessary to add set [these] in ->events"
	ee.events = EPOLLET | EPOLLPRI | eflags;
	if(rfxn){
		ee.events |= EVREAD;
	}
	if(tfxn){
		ee.events |= EVWRITE;
	}
	return Kevent(evq->efd,&k,1,NULL,0);
#elif defined(TORQUE_FREEBSD)
	struct kevent k[2];

	// FIXME enforce EPOLLONESHOT equivalent
	if(rfxn){
		EV_SET(&k[0],fd,EVREAD,EV_ADD | EVEDGET | eflags,0,0,NULL);
	}
	if(tfxn){
		EV_SET(&k[1],fd,EVWRITE,EV_ADD | EVEDGET | eflags,0,0,NULL);
	}
	return Kevent(evq->efd,k,!!tfxn + !!rfxn,NULL,0);
#else
#error "No fd event implementation on this OS"
#endif
	return 0;
}

int add_fd_to_evhandler(torque_ctx *ctx,const evqueue *evq,int fd,
			libtorquercb rfxn,libtorquewcb tfxn,
			void *cbstate,int eflags){
	if((unsigned)fd >= ctx->eventtables.fdarraysize){
		return -1;
	}
	setup_evsource(ctx->eventtables.fdarray,fd,rfxn,tfxn,cbstate);
	if(add_fd_event(evq,fd,rfxn,tfxn,eflags)){
		return -1;
	}
	return 0;
}
