#include <pthread.h>
#include <libtorque/events/fd.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/sources.h>

static inline libtorque_err
add_fd_event(const evqueue *evq,int fd,libtorquercb rfxn,libtorquewcb tfxn,int eflags){
#ifdef LIBTORQUE_LINUX
	struct epoll_ctl_data ecd;
	struct epoll_event ee;
	struct kevent k;

	if(eflags & ~(EPOLLONESHOT)){ // enforce allowed eflags
		return LIBTORQUE_ERR_INVAL;
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
		ee.events |= EPOLLIN;
	}
	if(tfxn){
		ee.events |= EPOLLOUT;
	}
	if(Kevent(evq->efd,&k,1,NULL,0)){
		return LIBTORQUE_ERR_ASSERT;
	}
#elif defined(LIBTORQUE_FREEBSD)
	struct kevent k[2];

	// FIXME enforce EPOLLONESHOT equivalent
	if(rfxn){
		EV_SET(&k[0],fd,EVFILT_READ,EV_ADD | EV_CLEAR | eflags,0,0,NULL);
	}
	if(tfxn){
		EV_SET(&k[1],fd,EVFILT_WRITE,EV_ADD | EV_CLEAR | eflags,0,0,NULL);
	}
	if(Kevent(evq->efd,k,!!tfxn + !!rfxn,NULL,1)){
		return LIBTORQUE_ERR_ASSERT;
	}
#else
#error "No fd event implementation on this OS"
#endif
	return 0;
}

libtorque_err add_fd_to_evhandler(libtorque_ctx *ctx,const evqueue *evq,int fd,
			libtorquercb rfxn,libtorquewcb tfxn,
			void *cbstate,int eflags){
	if((unsigned)fd >= ctx->eventtables.fdarraysize){
		return LIBTORQUE_ERR_INVAL;
	}
	setup_evsource(ctx->eventtables.fdarray,fd,rfxn,tfxn,cbstate);
	return add_fd_event(evq,fd,rfxn,tfxn,eflags);
}
