#include <unistd.h>
#include <libtorque/events/fd.h>
#include <libtorque/events/timer.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/sources.h>

#if defined(LIBTORQUE_LINUX_TIMERFD) || defined(LIBTORQUE_FREEBSD)
static inline timerfd_marshal *
create_timerfd_marshal(libtorquetimecb tfxn,void *cbstate){
	timerfd_marshal *ret;

	if( (ret = malloc(sizeof(*ret))) ){
		ret->tfxn = tfxn;
		ret->cbstate = cbstate;
	}
	return ret;
}

static inline void
timer_passthru(int fd __attribute__ ((unused)),void *state){
	timer_curry(state);
}
#endif

// from kevent(2) on FreeBSD 6.4:
// EVFILT_SIGNAL  Takes the signal number to monitor as the identifier and
// 	returns when the given signal is delivered to the process.
// 	This coexists with the signal() and sigaction() facili-
// 	ties, and has a lower precedence.  The filter will record
// 	all attempts to deliver a signal to a process, even if the
// 	signal has been marked as SIG_IGN.  Event notification
// 	happens after normal signal delivery processing.  data
// 	returns the number of times the signal has occurred since
// 	the last call to kevent().  This filter automatically sets
// 	the EV_CLEAR flag internally.
// from signalfd(2) on Linux 2.6.31:
//      The mask argument specifies the set of signals that the  caller  wishes
//      to accept via the file descriptor.  This argument is a signal set whose
//      contents can be initialized using the macros described in sigsetops(3).
//      Normally,  the  set  of  signals to be received via the file descriptor
//      should be blocked using sigprocmask(2), to prevent  the  signals  being
//      handled according to their default dispositions.  It is not possible to
//      receive SIGKILL or SIGSTOP signals  via  a  signalfd  file  descriptor;
//      these signals are silently ignored if specified in mask.
libtorque_err add_timer_to_evhandler(struct libtorque_ctx *ctx __attribute__ ((unused)),
		const struct evqueue *evq,const struct itimerspec *t,
		libtorquetimecb tfxn,void *cbstate){
#ifdef LIBTORQUE_LINUX_TIMERFD
	timerfd_marshal *tm;
	int fd;

	if((tm = create_timerfd_marshal(tfxn,cbstate)) == NULL){
		return LIBTORQUE_ERR_RESOURCE;
	}
	if((fd = timerfd_create(CLOCK_MONOTONIC,TFD_NONBLOCK | TFD_CLOEXEC)) < 0){
		free(tm);
		if(errno == EINVAL){
			return LIBTORQUE_ERR_INVAL;
		}else if(errno == ENODEV || errno == ENOSYS){
			return LIBTORQUE_ERR_UNAVAIL;
		}
		return LIBTORQUE_ERR_RESOURCE;
	}
	// FIXME need determine whether TFD_TIMER_ABSTIME ought be used
	if(timerfd_settime(fd,0,t,NULL)){
		close(fd);
		free(tm);
		return LIBTORQUE_ERR_INVAL;
	}
	if(add_fd_to_evhandler(ctx,evq,fd,timerfd_passthru,NULL,cbstate,0)){
		close(fd);
		free(tm);
		return LIBTORQUE_ERR_ASSERT;
	}
#elif defined(LIBTORQUE_LINUX)
//#error "Need Linux 2.6.25 / GNU libc 2.8 for timerfd"
	if(!ctx || !evq || !t || !tfxn || !cbstate){
		return LIBTORQUE_ERR_INVAL;
	}
	return LIBTORQUE_ERR_UNAVAIL; // FIXME working around compile
#elif defined(LIBTORQUE_FREEBSD)
	timerfd_marshal *tm;
	EVECTOR_AUTOS(1,tk);
	uintmax_t ms;

	ms = t->it_interval.tv_sec * 1000 + t->it_interval.tv_nsec / 1000000;
	if(!ms){
		return LIBTORQUE_ERR_INVAL;
	}
	if((tm = create_timerfd_marshal(tfxn,cbstate)) == NULL){
		return LIBTORQUE_ERR_RESOURCE;
	}
	EV_SET(tk.eventv,(uintptr_t)tm,EVFILT_TIMER,EV_ADD | EVONESHOT,0,ms,NULL);
	if(Kevent(evq->efd,tk.eventv,1,NULL,0)){
		free(tm);
		return LIBTORQUE_ERR_RESOURCE;
	}
#else
#error "No timer event implementation on this OS"
#endif
	return 0;
}
