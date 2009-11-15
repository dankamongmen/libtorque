#include <unistd.h>
#include <signal.h>
#include <libtorque/events/fds.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/signals.h>
#include <libtorque/events/sources.h>

#ifdef LIBTORQUE_LINUX
#include <sys/signalfd.h>
static int
signalfd_demultiplexer(int fd,void *cbstate){
	struct signalfd_siginfo si;
	evhandler *e = cbstate;
	int ret = 0;
	ssize_t r;

	do{
		if((r = read(fd,&si,sizeof(si))) == sizeof(si)){
			ret |= handle_evsource_read(e->evsources->sigarray,
							si.ssi_signo);
			// FIXME do... what, exactly with ret?
		}else if(r >= 0){
			// FIXME stat short read! return -1?
		}
	}while(r >= 0 && errno != EINTR);
	if(errno != EAGAIN){
		// FIXME stat on error reading signalfd! return -1?
	}
	return 0;
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
add_signal_event(evhandler *eh,const sigset_t *sigs,libtorquecb rfxn,void *cbstate){
	unsigned z;
#ifdef LIBTORQUE_LINUX
	{
		// FIXME we could restrict this all to a single signalfd, since
		// it takes a sigset_t...less potential parallelism, though
		int fd;

		if((fd = signalfd(-1,sigs,SFD_NONBLOCK | SFD_CLOEXEC)) < 0){
			return -1;
		}
		if(add_fd_to_evcore(eh,eh->externalvec,fd,
				signalfd_demultiplexer,NULL,eh)){
			close(fd);
			return -1;
		}
	}
#elif defined(LIBTORQUE_FREEBSD)
	{

		for(z = 1 ; z < eh->evsources->sigarraysize ; ++z){
			struct kevent k;

			if(!sigismember(sigs,z)){
				continue;
			}
			EV_SET(&k,z,EVFILT_SIGNAL,EV_ADD | EV_CLEAR,0,0,NULL);
			if(add_evector_kevents(ev,&k,1)){
				return -1;
			}
		}
	}
#else
#error "No signal event implementation on this OS"
#endif
	for(z = 1 ; z < eh->evsources->sigarraysize ; ++z){
		if(sigismember(sigs,z)){
			setup_evsource(eh->evsources->sigarray,z,rfxn,NULL,cbstate);
		}
	}
	return 0;
}

int add_signal_to_evhandler(evhandler *eh,const sigset_t *sigs,
				libtorquecb rfxn,void *cbstate){
	if(pthread_mutex_lock(&eh->lock) == 0){
		if(add_signal_event(eh,sigs,rfxn,cbstate) == 0){
			// flush_evector_changes unlocks on all paths
			return flush_evector_changes(eh,eh->externalvec);
		}
		pthread_mutex_unlock(&eh->lock);
	}
	return -1;
}
