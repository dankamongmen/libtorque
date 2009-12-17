#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/resource.h>
#include <libtorque/events/fd.h>
#include <libtorque/events/evq.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/signal.h>
#include <libtorque/events/sources.h>

static __thread libtorque_ctx *tsd_ctx;
static __thread evhandler *tsd_evhandler;

evhandler *get_thread_evh(void){
	return tsd_evhandler;
}

libtorque_ctx *get_thread_ctx(void){
	return tsd_ctx;
}

static void
handle_event(evhandler *eh,const kevententry *e){
	int ret = 0;

#ifdef LIBTORQUE_LINUX
	if(e->events & EPOLLIN){
#else
	if(e->filter == EVFILT_READ){
#endif
		ret |= handle_evsource_read(eh->evsources->fdarray,KEVENTENTRY_FD(e));
	}
#ifdef LIBTORQUE_LINUX
	if(e->events & EPOLLOUT){
#else
	if(e->filter == EVFILT_WRITE){
#endif
		ret |= handle_evsource_write(eh->evsources->fdarray,KEVENTENTRY_FD(e));
	}
}

static int
rxcommonsignal(int sig,libtorque_cbctx *nullv __attribute__ ((unused)),
				void *cbstate __attribute__ ((unused))){
	if(sig == EVTHREAD_TERM){
		void *ret = PTHREAD_CANCELED;
		evhandler *e = tsd_evhandler;
		int r;

		// FIXME this is kind of lame. I'd like to emulate
		// RUSAGE_THREAD when it's unavailable, and either way the test
		// ought be based on whether RUSAGE_THREAD is *expected*, not
		// *defined* (Linux 2.6.26+) for future-proofing.
#ifdef RUSAGE_THREAD
		struct rusage ru;
		getrusage(RUSAGE_THREAD,&ru);
		e->stats.utimeus = ru.ru_utime.tv_sec * 1000000 + ru.ru_utime.tv_usec;
		e->stats.stimeus = ru.ru_stime.tv_sec * 1000000 + ru.ru_stime.tv_usec;
#endif
		e->stats.utimeus = e->stats.stimeus = 0;
		--e->stats.events;
		// There's no POSIX thread cancellation going on here, nor are
		// we terminating due to signal; we're catching the signal and
		// exiting from this thread only. The trigger signal might be
		// delivered to any one of our threads; if we're here, though,
		// we cannot be holding the efd. Progress is thus assured.
		pthread_kill(e->nexttid,EVTHREAD_TERM);
		// We rely on EDEADLK to cut off our circular join()list
		if((r = pthread_join(e->nexttid,&ret)) && r != EDEADLK){
			ret = NULL;
		}
		if(destroy_evhandler(e)){
			ret = NULL;
		}
		pthread_exit(ret);
	}
	// FIXME else?
	return 0;
}

void event_thread(libtorque_ctx *ctx,evhandler *e){
	tsd_evhandler = e;
	tsd_ctx = ctx;
	while(1){
		int events;

		events = Kevent(e->evq->efd,PTR_TO_CHANGEV(&e->evec),e->evec.changesqueued,
					PTR_TO_EVENTV(&e->evec),e->evec.vsizes);
		if(events <= 0){
			continue;
		}
		e->stats.events += events;
		do{
#ifdef LIBTORQUE_LINUX
		handle_event(e,&PTR_TO_EVENTV(&e->evec)->events[--events]);
#else
		handle_event(e,&PTR_TO_EVENTV(&e->evec)[--events]);
#endif
		}while(events);
		++e->stats.rounds;
	}
}

static int
#ifdef LIBTORQUE_LINUX
create_evector(struct kevent *kv,int n){
	if((kv->events = malloc(n * sizeof(*kv->events))) == NULL){
		return -1;
	}
	if((kv->ctldata = malloc(n * sizeof(*kv->ctldata))) == NULL){
		free(kv->events);
		return -1;
	}
	return 0;
#else
create_evector(struct kevent **kv,int n){
	if((*kv = malloc(n * sizeof(**kv))) == NULL){
		return -1;
	}
	return 0;
#endif
}

static void
#ifdef LIBTORQUE_LINUX
destroy_evector(struct kevent *kv){
	free(kv->events);
	free(kv->ctldata);
#else
destroy_evector(struct kevent **kv){
	free(*kv);
#endif
}

static int
init_evectors(evectors *ev){
	// We probably want about a half (small) page's worth...? FIXME
	ev->vsizes = 512;
	if(create_evector(&ev->eventv,ev->vsizes)){
		return -1;
	}
	if(create_evector(&ev->changev,ev->vsizes)){
		destroy_evector(&ev->eventv);
		return -1;
	}
	ev->changesqueued = 0;
	return 0;
}

static void
destroy_evectors(evectors *e){
	if(e){
		destroy_evector(&e->changev);
		destroy_evector(&e->eventv);
	}
}

static inline int
prep_common_sigset(sigset_t *s){
	return sigemptyset(s) || sigaddset(s,EVTHREAD_TERM);
}

// All event queues (evqueues) will need to register events on the common
// signals (on Linux, this is done via a common signalfd()). Either way, we
// don't want to touch the evsources more than once.
int initialize_common_sources(struct evtables *evt){
	sigset_t s;

	if(prep_common_sigset(&s)){
		return -1;
	}
	if(EVTHREAD_TERM >= evt->sigarraysize){
		return -1;
	}
	setup_evsource(evt->sigarray,EVTHREAD_TERM,rxcommonsignal,NULL,NULL,NULL);
#ifdef LIBTORQUE_LINUX
	if((evt->common_signalfd = signalfd(-1,&s,SFD_NONBLOCK | SFD_CLOEXEC)) < 0){
		return -1;
	}
	setup_evsource(evt->fdarray,evt->common_signalfd,signalfd_demultiplexer,NULL,NULL,NULL);
#endif
	return 0;
}

static int
initialize_evhandler(evhandler *e,evtables *evsources,evqueue *evq){
	memset(e,0,sizeof(*e));
	e->evsources = evsources;
	e->evq = evq;
	if(init_evectors(&e->evec)){
		return -1;
	}
	return 0;
}

int create_efd(void){
	int fd,flags;

// Until the epoll API stabilizes a bit... :/
#ifdef LIBTORQUE_LINUX
#ifdef EPOLL_CLOEXEC
#define SAFE_EPOLL_CLOEXEC EPOLL_CLOEXEC
#else // otherwise, it wants a size hint in terms of fd's
#include <linux/limits.h>
#define SAFE_EPOLL_CLOEXEC NR_OPEN
#endif
	if((fd = epoll_create(SAFE_EPOLL_CLOEXEC)) < 0){
		return -1;
	}
	if(SAFE_EPOLL_CLOEXEC == 0){
		if(((flags = fcntl(fd,F_GETFD)) < 0) ||
				fcntl(fd,F_SETFD,flags | FD_CLOEXEC)){
			close(fd);
			return -1;
		}
	}
#undef SAFE_EPOLL_CLOEXEC
#elif defined(LIBTORQUE_FREEBSD)
	if((fd = kqueue()) < 0){
		return -1;
	}
	if(((flags = fcntl(fd,F_GETFD)) < 0) || fcntl(fd,F_SETFD,flags | FD_CLOEXEC)){
		close(fd);
		return -1;
	}
#endif
	return fd;
}

evhandler *create_evhandler(evtables *evsources,evqueue *evq){
	evhandler *ret;

	if( (ret = malloc(sizeof(*ret))) ){
		if(initialize_evhandler(ret,evsources,evq) == 0){
			return ret;
		}
		free(ret);
	}
	return NULL;
}

static void print_evstats(const evthreadstats *stats){
#define PRINTSTAT(s,field) \
	do { if((s)->field){ printf(#field ": %ju\n",(s)->field); } }while(0)
#define STATDEF(field) PRINTSTAT(stats,field);
#include <libtorque/events/x-stats.h>
#undef STATDEF
#undef PRINTSTAT
}

int destroy_evhandler(evhandler *e){
	int ret = 0;

	if(e){
		print_evstats(&e->stats);
		destroy_evectors(&e->evec);
		ret |= destroy_evqueue(e->evq);
		free(e);
	}
	return ret;
}
