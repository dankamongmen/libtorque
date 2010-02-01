#include <unistd.h>
#include <signal.h>
#include <libtorque/events/fd.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/signal.h>
#include <libtorque/events/sources.h>

void signal_demultiplexer(int s){
	torque_ctx *ctx = get_thread_ctx();
	int errdup;

	// Preserve errno (in case the callback changes it and we're being
	// handled right before a system call -- this shouldn't happen for us).
	errdup = errno;
	// If we're called from another thread (signal handlers are shared,
	// and it's possible that the client fails to mask one of our signals),
	// ev will be NULL and we oughtn't process the event. that shouldn't
	// happen, except due to user error.
	if(ctx){
		evhandler *ev = get_thread_evh();

		++ev->stats.events;
		handle_evsource_read(ctx->eventtables.sigarray,s);
	}
	errno = errdup;
}

int init_signal_handlers(void){
	struct sigaction act;

	memset(&act,0,sizeof(act));
	act.sa_handler = signal_demultiplexer;
	sigfillset(&act.sa_mask);
	if(sigaction(EVTHREAD_TERM,&act,NULL) || sigaction(EVTHREAD_INT,&act,NULL)){
		return -1;
	}
	return 0;
}

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
torque_err add_signal_to_evhandler(torque_ctx *ctx,const evqueue *evq __attribute__ ((unused)),
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
		torque_err ret;
		int fd;

		if((fd = signalfd(-1,sigs,SFD_NONBLOCK | SFD_CLOEXEC)) < 0){
			if(errno == ENOSYS){
				return TORQUE_ERR_UNAVAIL;
			}
			return TORQUE_ERR_RESOURCE;
		}
		if( (ret = add_fd_to_evhandler(ctx,evq,fd,signalfd_demultiplexer,NULL,ctx,0)) ){
			close(fd);
			return ret;
		}
	}
#elif defined(LIBTORQUE_LINUX)
	if(add_epoll_sigset(sigs,ctx->eventtables.sigarraysize)){
		return TORQUE_ERR_ASSERT;
	}
#elif defined(LIBTORQUE_FREEBSD)
	for(z = 1 ; z < ctx->eventtables.sigarraysize ; ++z){
		struct kevent k;

		if(!sigismember(sigs,z)){
			continue;
		}
		EV_SET(&k,z,EVFILT_SIGNAL,EV_ADD | EVEDGET,0,0,NULL);
		if(Kevent(evq->efd,&k,1,NULL,0)){
			return TORQUE_ERR_ASSERT; // FIXME pull previous out
		}
	}
#else
#error "No signal event implementation on this OS"
#endif
	return 0;
}
