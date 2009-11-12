#include <libdank/utils/threads.h>
#include <libdank/ersatz/compat.h>
#include <libdank/utils/syswrap.h>
#include <libdank/modules/events/fds.h>
#include <libdank/modules/events/evcore.h>
#include <libdank/modules/events/signals.h>
#include <libdank/modules/events/sources.h>

#ifdef LIB_COMPAT_LINUX
#include <sys/signalfd.h>
static void
signalfd_demultiplexer(int fd,void *cbstate){
	struct signalfd_siginfo si;
	evhandler *e = cbstate;
	ssize_t r;

	do{
		if((r = read(fd,&si,sizeof(si))) == sizeof(si)){
			handle_evsource_read(e->sigarray,si.ssi_signo);
		}else if(r >= 0){
			bitch("Got short read (%zd) off signalfd %d\n",r,fd);
			// FIXME stat!
		}
	}while(r >= 0 && errno != EINTR);
	if(errno != EAGAIN){
		moan("Error reading from signalfd %d\n",fd);
	}
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
static inline int
add_signal_event(evhandler *eh,struct evectors *ev,int sig,
				evcbfxn rfxn,void *cbstate){
	sigset_t mask,oldmask;

	sigemptyset(&mask);
	if(Sigaddset(&mask,sig)){
		return -1;
	}
	// FIXME actually we want to set the procmask of evhandler threads
	// present and future...not necessarily the entire process. hrmmmm...
	if(pthread_sigmask(SIG_BLOCK,&mask,&oldmask)){
		return -1;
	}
	if(!sigismember(&oldmask,sig)){
		return -1;
	}
#ifdef LIB_COMPAT_LINUX
	{
		// FIXME we could restrict this all to a single signalfd, since
		// it takes a sigset_t...less potential parallelism, though
		int fd;

		if((fd = signalfd(-1,&mask,SFD_NONBLOCK | SFD_CLOEXEC)) < 0){
			return -1;
		}
		if(add_fd_to_evcore(eh,ev,fd,signalfd_demultiplexer,NULL,eh)){
			Close(fd);
			return -1;
		}
	}
#else
#ifdef LIB_COMPAT_FREEBSD
	{
		struct kevent k;

		EV_SET(&k,sig,EVFILT_SIGNAL,EV_ADD | EV_CLEAR,0,0,NULL);
		if(add_evector_kevents(ev,&k,1)){
			return -1;
		}
	}
#else
#error "No signal event implementation on this OS"
#endif
#endif
	setup_evsource(eh->sigarray,sig,rfxn,NULL,cbstate);
	return 0;
}

int add_signal_to_evcore(evhandler *eh,struct evectors *ev,int sig,
				evcbfxn rfxn,void *cbstate){
	if(sig >= eh->sigarraysize){
		return -1;
	}
	if(add_signal_event(eh,ev,sig,rfxn,cbstate)){
		return -1;
	}
	return 0;
}

int add_signal_to_evhandler(evhandler *eh,int sig,evcbfxn rfxn,void *cbstate){
	struct evectors *ev = eh->externalvec;

	if(pthread_mutex_lock(&eh->lock) == 0){
		if(add_signal_to_evcore(eh,ev,sig,rfxn,cbstate) == 0){
			if(flush_evector_changes(eh,ev) == 0){
				return pthread_mutex_unlock(&eh->lock);
			}
		}
		pthread_mutex_unlock(&eh->lock);
	}
	return -1;
}
