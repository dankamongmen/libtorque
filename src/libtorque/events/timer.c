#include <unistd.h>
#include <libtorque/events/fd.h>
#include <libtorque/events/timer.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/sources.h>

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
int add_timer_to_evhandler(struct libtorque_ctx *ctx,evhandler *eh,
		const struct itimerspec *t,libtorquercb rfxn,void *cbstate){
#ifdef LIBTORQUE_LINUX_TIMERFD
	int fd;

	if((fd = timerfd_create(CLOCK_MONOTONIC,TFD_NONBLOCK | TFD_CLOEXEC)) < 0){
		return -1;
	}
	if(timerfd_settime(fd,0,t,NULL)){
		close(fd);
		return -1;
	}
	if(add_fd_to_evhandler(ctx,eh,fd,rfxn,NULL,NULL,cbstate,0)){
		close(fd);
		return -1;
	}
#elif defined(LIBTORQUE_LINUX)
//#error "Need Linux 2.6.25 / GNU libc 2.8 for timerfd"
	if(!eh || !t || !rfxn || !cbstate){
		return -1;
	}
	return -1; // FIXME working around compile
#elif defined(LIBTORQUE_FREEBSD)
#error "Need to implement FreeBSD timerfd"
#else
#error "No timer event implementation on this OS"
#endif
	return 0;
}
