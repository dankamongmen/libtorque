#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/resource.h>
#include <libtorque/events/fd.h>
#include <libtorque/events/evq.h>
#include <libtorque/events/timer.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/signal.h>
#include <libtorque/events/sources.h>
#include <libtorque/hardware/topology.h>

static __thread libtorque_ctx *tsd_ctx;
static __thread evhandler *tsd_evhandler;

evhandler *get_thread_evh(void){
	return tsd_evhandler;
}

libtorque_ctx *get_thread_ctx(void){
	return tsd_ctx;
}

static inline void
handle_event(libtorque_ctx *ctx,const kevententry *e){
#ifdef LIBTORQUE_LINUX
	if(e->events & EPOLLIN){
#else
	if(e->filter == EVFILT_READ){
#endif
		handle_evsource_read(ctx->eventtables.fdarray,KEVENTENTRY_ID(e));
	}
#ifdef LIBTORQUE_LINUX
	if(e->events & EPOLLOUT){
#else
	else if(e->filter == EVFILT_WRITE){
#endif
		handle_evsource_write(ctx->eventtables.fdarray,KEVENTENTRY_ID(e));
	}
#ifdef LIBTORQUE_FREEBSD
	else if(e->filter == EVFILT_SIGNAL){
		handle_evsource_read(ctx->eventtables.sigarray,KEVENTENTRY_ID(e));
        }else if(e->filter == EVFILT_TIMER){
		timer_curry(KEVENTENTRY_IDPTR(e));
	}
#endif
}

void rxcommonsignal(int sig,void *cbstate){
	if(sig == EVTHREAD_TERM || sig == EVTHREAD_INT){
		const libtorque_ctx *ctx = cbstate;
		void *ret = PTHREAD_CANCELED;
		evhandler *e = get_thread_evh();
		struct rusage ru;
		int r;

		// There's no POSIX thread cancellation going on here, nor are
		// we terminating due to signal; we're catching the signal and
		// exiting from this thread only. The trigger signal might be
		// delivered to any one of our threads; if we're here, though,
		// we cannot be holding the efd. Progress is thus assured.
		pthread_kill(e->nexttid,sig);
		// We rely on EDEADLK to cut off our circular join()list
		if((r = pthread_join(e->nexttid,&ret)) && r != EDEADLK){
			ret = NULL;
		}
		// FIXME this is kind of lame. I'd like to emulate
		// RUSAGE_THREAD when it's unavailable, and either way the test
		// ought be based on whether RUSAGE_THREAD is *expected*, not
		// *defined* (Linux 2.6.26+) for future-proofing.
#ifdef RUSAGE_THREAD
		getrusage(RUSAGE_THREAD,&ru);
#else
		getrusage(RUSAGE_SELF,&ru);
#endif
		e->stats.utimeus = ru.ru_utime.tv_sec * 1000000 + ru.ru_utime.tv_usec;
		e->stats.stimeus = ru.ru_stime.tv_sec * 1000000 + ru.ru_stime.tv_usec;
		e->stats.vctxsw = ru.ru_nvcsw;
		e->stats.ictxsw = ru.ru_nivcsw;
		destroy_evhandler(ctx,e);
		pthread_exit(ret); // FIXME need clean up stack
	}
}

#if defined(LIBTORQUE_LINUX) && !defined(LIBTORQUE_LINUX_SIGNALFD)
static sig_atomic_t sem_rxcommonsignal;

static inline void
check_for_termination(void){
	if(sem_rxcommonsignal){
		int s;

		s = sem_rxcommonsignal;
		sem_rxcommonsignal = 0;
		rxcommonsignal(s,get_thread_ctx());
	}
}

static void
rxcommonsignal_handler(int sig,void *cbstate __attribute__ ((unused))){
	sem_rxcommonsignal = sig;
}
#else
#define check_for_termination(...)
#endif

void event_thread(libtorque_ctx *ctx,evhandler *e){
	tsd_evhandler = e;
	tsd_ctx = ctx;
	while(1){
		int events;

		check_for_termination();
		events = Kevent(e->evq->efd,NULL,0,PTR_TO_EVENTV(&e->evec),e->evec.vsizes);
		++e->stats.rounds;
		if(events < 0){
			if(errno != EINTR){
				++e->stats.pollerr;
			}
			continue;
		}
		while(events--){
#ifdef LIBTORQUE_LINUX
			handle_event(ctx,&PTR_TO_EVENTV(&e->evec)->events[events]);
#else
			handle_event(ctx,&PTR_TO_EVENTV(&e->evec)[events]);
#endif
			++e->stats.events;
		}
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
	return 0;
}

static void
destroy_evectors(evectors *e){
	if(e){
		destroy_evector(&e->eventv);
	}
}

static inline int
prep_common_sigset(sigset_t *s){
	return sigemptyset(s) || sigaddset(s,EVTHREAD_TERM) ||
			sigaddset(s,EVTHREAD_INT);
}

// All event queues (evqueues) will need to register events on the common
// signals (on Linux, this is done via a common signalfd()). Either way, we
// don't want to touch the evsources more than once.
int initialize_common_sources(libtorque_ctx *ctx,struct evtables *evt,const sigset_t *ss __attribute__ ((unused))){
	if(EVTHREAD_TERM >= evt->sigarraysize || EVTHREAD_INT >= evt->sigarraysize){
		return -1;
	}
#ifdef LIBTORQUE_LINUX_SIGNALFD
	{
	sigset_t s;

	if(prep_common_sigset(&s)){
		return -1;
	}
	if((evt->common_signalfd = signalfd(-1,&s,SFD_NONBLOCK | SFD_CLOEXEC)) < 0){
		return -1;
	}
	setup_evsource(evt->fdarray,evt->common_signalfd,signalfd_demultiplexer,NULL,ctx);
	setup_evsource(evt->sigarray,EVTHREAD_TERM,rxcommonsignal,NULL,ctx);
	setup_evsource(evt->sigarray,EVTHREAD_INT,rxcommonsignal,NULL,ctx);
	}
#elif defined(LIBTORQUE_LINUX)
	setup_evsource(evt->sigarray,EVTHREAD_TERM,rxcommonsignal_handler,NULL,ctx);
	setup_evsource(evt->sigarray,EVTHREAD_INT,rxcommonsignal_handler,NULL,ctx);
	if(init_epoll_sigset(ss)){
		return -1;
	}
#else
	setup_evsource(evt->sigarray,EVTHREAD_TERM,rxcommonsignal,NULL,ctx);
	setup_evsource(evt->sigarray,EVTHREAD_INT,rxcommonsignal,NULL,ctx);
#endif
	if(init_signal_handlers()){
		return -1;
	}
	return 0;
}

static int
initialize_evhandler(evhandler *e,const evqueue *evq,const stack_t *stack){
	memset(e,0,sizeof(*e));
	e->stats.stackptr = stack->ss_sp;
	e->stats.stacksize = stack->ss_size;
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
#define SAFE_EPOLL_CLOEXEC sysconf(_SC_OPEN_MAX)
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

evhandler *create_evhandler(const evqueue *evq,const stack_t *stack){
	evhandler *ret;

	if( (ret = malloc(sizeof(*ret))) ){
		if(initialize_evhandler(ret,evq,stack) == 0){
			return ret;
		}
		free(ret);
	}
	return NULL;
}

static inline int
print_evthread(FILE *fp,const libtorque_ctx *ctx){
	const libtorque_cput *cpu;
	int aid;

	if((aid = get_thread_aid()) < 0){
		return -1;
	}
	if((cpu = lookup_aid(ctx,aid)) == NULL){
		return -1;
	}
	if(fprintf(fp,"aid=\"%d\" cputype=\"%tu\"",aid,cpu - ctx->cpudescs) < 0){
		return -1;
	}
	return 0;
}

static inline int
print_evstats(const libtorque_ctx *ctx,const evthreadstats *stats){
	if(printf("<thread ") < 0){
		return -1;
	}
	if(print_evthread(stdout,ctx)){
		return -1;
	}
	if(printf(">") < 0){
		return -1;
	}
#define PRINTSTAT(s,field,format) \
 do { if((s)->field){ if(printf("<" #field ">%" format "</" #field ">",(s)->field) < 0){ return -1; } } }while(0)
#define STATDEF(field) PRINTSTAT(stats,field,"ju");
#define PTRDEF(field) PRINTSTAT(stats,field,"p");
#include <libtorque/events/x-stats.h>
#undef PTRDEF
#undef STATDEF
#undef PRINTSTAT
	if(printf("</thread>\n") < 0){
		return -1;
	}
	return 0;
}

void destroy_evhandler(const libtorque_ctx *ctx,evhandler *e){
	if(e){
		print_evstats(ctx,&e->stats);
		destroy_evectors(&e->evec);
		free(e);
	}
}
