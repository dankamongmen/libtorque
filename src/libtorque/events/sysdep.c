#include <string.h>
#include <unistd.h>
#include <libtorque/events/evq.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/sources.h>

// We do not enforce, but do expect and require:
//  - EV_ADD/EPOLL_CTL_ADD to be used as the control operation
//  - EPOLLET/EV_CLEAR to be used in the flags
int add_evector_kevents(const evqueue *evq,struct kevent *k,int kcount){
	if(kcount <= 0){
		return -1;
	}
	return Kevent(evq->efd,k,kcount,NULL,0);
}

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

static void
signal_demultiplexer(int s){
	libtorque_ctx *ctx = get_thread_ctx();

	// If we're called from another thread (signal handlers are process-wide,
	// and it's possible that the client fails to mask one of our signals),
	// ev will be NULL and we oughtn't process the event. that shouldn't
	// happen, except due to user error.
	if(ctx){
		evhandler *ev = get_thread_evh();

		++ev->stats.events;
		handle_evsource_read(ctx->eventtables.sigarray,s);
	}
}

int init_epoll_sigset(const sigset_t *ss){
	struct sigaction act;

	/* Signals which were blocked on entry ought remain blocked throughout,
	 * since we can assume the calling application had some reason to do so
	 * (most likely that we want to synchronously receive said signal). */
	memcpy(&epoll_sigset_base,ss,sizeof(*ss));
	if(sigdelset(&epoll_sigset_base,EVTHREAD_TERM) ||
			sigdelset(&epoll_sigset_base,EVTHREAD_INT)){
		return -1;
	}
	memset(&act,0,sizeof(act));
	act.sa_handler = signal_demultiplexer;
	sigfillset(&act.sa_mask);
	if(sigaction(EVTHREAD_TERM,&act,NULL) ||
			sigaction(EVTHREAD_INT,&act,NULL)){
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
	struct epoll_event ee;

	// eflags should be union over *only* { 0, EPOLLIN, EPOLLOUT }
	memset(&ee,0,sizeof(ee));
	// EPOLLRDHUP isn't available prior to kernel 2.6.17 and GNU libc 2.6.
	// We shouldn't need it, though.
	ee.events = EPOLLET | EPOLLONESHOT | eflags;
	ee.data.fd = fd;
	if(epoll_ctl(evh->evq->efd,EPOLL_CTL_MOD,fd,&ee)){
		return -1;
	}
	return 0;
}
