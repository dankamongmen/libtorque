#include <unistd.h>
#include <signal.h>
#include <libtorque/events/fd.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/signal.h>
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
//
int add_signal_to_evhandler(libtorque_ctx *ctx,const evqueue *evq __attribute__ ((unused)),
			const sigset_t *sigs,libtorquercb rfxn,void *cbstate){
	unsigned z;

	for(z = 1 ; z < ctx->eventtables.sigarraysize ; ++z){
		if(sigismember(sigs,z)){
			setup_evsource(ctx->eventtables.sigarray,z,rfxn,
					NULL,cbstate);
		}
	}
#ifdef LIBTORQUE_LINUX_SIGNALFD
	{
		// FIXME we could restrict this all to a single signalfd, since
		// it takes a sigset_t...less potential parallelism, though
		int fd;

		if((fd = signalfd(-1,sigs,SFD_NONBLOCK | SFD_CLOEXEC)) < 0){
			return -1;
		}
		if(add_fd_to_evhandler(ctx,evq,fd,signalfd_demultiplexer,NULL,NULL,0)){
			close(fd);
			return -1;
		}
	}
#elif defined(LIBTORQUE_LINUX)
	if(add_epoll_sigset(sigs,ctx->eventtables.sigarraysize)){
		return -1;
	}
#elif defined(LIBTORQUE_FREEBSD)
	for(z = 1 ; z < ctx->eventtables.sigarraysize ; ++z){
		struct kevent k;

		if(!sigismember(sigs,z)){
			continue;
		}
		EV_SET(&k,z,EVFILT_SIGNAL,EV_ADD | EV_CLEAR,0,0,NULL);
		if(add_evector_kevents(evq,&k,1)){
			return -1; // FIXME go back and purge any we added?
		}
	}
#else
#error "No signal event implementation on this OS"
#endif
	return 0;
}
