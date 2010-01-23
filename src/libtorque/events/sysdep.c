#include <string.h>
#include <unistd.h>
#include <libtorque/events/evq.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/signal.h>
#include <libtorque/events/sources.h>

#ifdef LIBTORQUE_LINUX_SIGNALFD
void signalfd_demultiplexer(int fd,void *cbstate){
	const libtorque_ctx *ctx = cbstate;
	struct signalfd_siginfo si;
	ssize_t r;

	do{
		if((r = read(fd,&si,sizeof(si))) == sizeof(si)){
			evhandler *e = get_thread_evh();

			++e->stats.events;
			handle_evsource_read(ctx->eventtables.sigarray,
							si.ssi_signo);
		}else if(r >= 0){
			// FIXME stat short read!
		}
	}while(r >= 0 || errno == EINTR);
	if(errno != EAGAIN && errno != EWOULDBLOCK){
		// FIXME stat on error reading signalfd!
	}
}
#elif defined(LIBTORQUE_LINUX)
// FIXME ack, these need to be per-context, not global!
static sigset_t epoll_sigset_base;
const sigset_t *epoll_sigset = &epoll_sigset_base;
static pthread_mutex_t epoll_sigset_lock = PTHREAD_MUTEX_INITIALIZER;

int init_epoll_sigset(const sigset_t *ss){
	/* Signals which were blocked on entry ought remain blocked throughout,
	 * since we can assume the calling application had some reason to do so
	 * (most likely that we want to synchronously receive said signal). */
	memcpy(&epoll_sigset_base,ss,sizeof(*ss));
	if(sigdelset(&epoll_sigset_base,EVTHREAD_TERM) ||
			sigdelset(&epoll_sigset_base,EVTHREAD_INT)){
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

int restorefd(const struct evhandler *evh,int fd,int eflags){
	EVECTOR_AUTOS(1,ev);

#ifdef LIBTORQUE_LINUX
	memset(&ev.eventv.events[0],0,sizeof(ev.eventv.events[0]));
	ev.eventv.events[0].events = EVEDGET | EVONESHOT | eflags;
	ev.eventv.events[0].data.fd = fd;
	ev.eventv.ctldata[0].op = EPOLL_CTL_MOD;
#elif defined(LIBTORQUE_FREEBSD)
	ev.eventv[0].filter = eflags;
	ev.eventv[0].flags = EVEDGET | EVONESHOT;
	ev.eventv[0].ident = fd;
#endif
	if(Kevent(evh->evq->efd,PTR_TO_EVENTV(&ev),1,NULL,0)){
		return -1;
	}
	return 0;
}
