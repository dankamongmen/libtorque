#include <string.h>
#include <unistd.h>
#include <libtorque/events/evq.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/sources.h>

// We do not enforce, but do expect and require:
//  - EV_ADD/EPOLL_CTL_ADD to be used as the control operation
//  - EPOLLET/EV_CLEAR to be used in the flags
int add_evector_kevents(evectors *e,const struct kevent *k,int kcount){
	unsigned kc;

	if(kcount < 0){
		return -1;
	}
	if(e->changesqueued + kcount > e->vsizes){
		// FIXME be more robust. try to flush the changes to the
		// evhandler, or reallocate larger vectors. at least add stat!
		return -1;
	}
	kc = (unsigned)kcount;
#ifdef LIBTORQUE_LINUX
	memcpy(&e->changev.ctldata[e->changesqueued],k->ctldata,
			sizeof(*k->ctldata) * kc);
	memcpy(&e->changev.events[e->changesqueued],k->events,
			sizeof(*k->events) * kc);
#elif defined(LIBTORQUE_FREEBSD)
	memcpy(&e->changev[e->changesqueued],k,sizeof(*k) * kc);
#else
#error "Missing OS support"
#endif
	e->changesqueued += kcount;
	return 0;
}

int flush_evector_changes(evhandler *eh,evectors *ev){
	return flush_evqueue_changes(eh->evq,ev);
}

#ifdef LIBTORQUE_LINUX_SIGNALFD
int signalfd_demultiplexer(int fd,libtorque_cbctx *cbctx __attribute__ ((unused)),
			void *cbstate __attribute__ ((unused))){
	struct signalfd_siginfo si;
	int ret = 0;
	ssize_t r;

	do{
		if((r = read(fd,&si,sizeof(si))) == sizeof(si)){
			evhandler *e = get_thread_evh();

			++e->stats.events;
			ret |= handle_evsource_read(e->evsources->sigarray,si.ssi_signo);
			// FIXME do... what, exactly with ret?
		}else if(r >= 0){
			// FIXME stat short read! return -1?
		}
	}while(r >= 0 || errno == EINTR);
	if(errno != EAGAIN){
		// FIXME stat on error reading signalfd! return -1?
	}
	return 0;
}
#elif defined(LIBTORQUE_LINUX)
// FIXME ack, these need to be per-context, not global!
static sigset_t epoll_sigset_base;
const sigset_t *epoll_sigset = &epoll_sigset_base;
static pthread_mutex_t epoll_sigset_lock = PTHREAD_MUTEX_INITIALIZER;

static void
signal_demultiplexer(int s){
	evhandler *ev = get_thread_evh();

	// If we're called from another thread (signal handlers are process-wide,
	// and it's possible that the client fails to mask one of our signals),
	// ev will be NULL and we oughtn't process the event. this shouldn't
	// be happening except due to user error. we ought track it, though...
	if(ev){
		++ev->stats.events;
		handle_evsource_read(ev->evsources->sigarray,s);
	}
}

int init_epoll_sigset(void){
	struct sigaction act;

	/* FIXME we'd like to use sigfillset(), so that we're blocking any
	   signal not explicitly registered with us (mod TERM/KILL/STOP).
	   unfortunately, that doesn't affect a currently-blocked epoll_wait(),
	   and thus if only signals are activated as event sources, they'll
	   never actually get called. we could use an internal wakeup signal
	   (or add some state to the SIGTERM handler, rxcommonsignal())...
	   though, this is actually the more natural semantics, and there's an
	   argument that they're the correct ones (if they didn't explicitly
	   have the signal blocked by the time we're called, who knows what all
	   else got done?). it's not a huge deal, merely a complex one. also,
	   note that we're coming in with a fully-masked set due to
	   libtorque_init_masked()'s call chain. to actually get at the calling
	   process's entry signal mask, we'd need pass it down.
	if(sigfillset(&epoll_sigset_base)){
		return -1;
	}
	if(sigdelset(&epoll_sigset_base,EVTHREAD_TERM)){
		return -1;
	}*/
	memset(&act,0,sizeof(act));
	act.sa_handler = signal_demultiplexer;
	sigfillset(&act.sa_mask);
	if(sigaction(EVTHREAD_TERM,&act,NULL)){
		return -1;
	}
	return 0;
}

// A bit of a misnomer; we actually *delete* the specified signals from the
// epoll_sigset mask, not add them.
int add_epoll_sigset(const sigset_t *s,unsigned maxsignal){
	struct sigaction act;
	int ret = 0;
	unsigned z;

	memset(&act,0,sizeof(act));
	act.sa_handler = signal_demultiplexer;
	sigfillset(&act.sa_mask);
	if(pthread_mutex_lock(&epoll_sigset_lock)){
		return -1;
	}
	for(z = 1 ; z < maxsignal ; ++z){
		if(!sigismember(s,z)){
			continue;
		}
		if(sigaction(z,&act,NULL)){
			ret = -1;
		}else if(sigdelset(&epoll_sigset_base,z)){
			ret = -1;
		}
	}
	pthread_mutex_unlock(&epoll_sigset_lock);
	return ret;
}
#endif
