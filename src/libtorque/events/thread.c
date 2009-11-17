#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/signals.h>
#include <libtorque/events/sources.h>

static __thread evhandler *tsd_evhandler;

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
rxsignal(int sig,void *nullv __attribute__ ((unused))){
	if(sig == EVTHREAD_TERM){
		evhandler *e = tsd_evhandler;

		destroy_evhandler(e);
		pthread_exit(PTHREAD_CANCELED);
	}
	return 0;
}

void event_thread(evhandler *e){
	tsd_evhandler = e;
	while(1){
		evectors *ev = e->externalvec; // FIXME really? surely not.
		int events,z;

		events = Kevent(e->efd,PTR_TO_CHANGEV(ev),ev->changesqueued,
					PTR_TO_EVENTV(ev),ev->vsizes);
		for(z = 0 ; z < events ; ++z){
#ifdef LIBTORQUE_LINUX
			handle_event(e,&PTR_TO_EVENTV(ev)->events[z]);
#else
			handle_event(e,&PTR_TO_EVENTV(ev)[z]);
#endif
		}
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

static evectors *
create_evectors(void){
	evectors *ret;

	if((ret = malloc(sizeof(*ret))) == NULL){
		return NULL;
	}
	// We probably want about a half (small) page's worth...? FIXME
	ret->vsizes = 512;
	if(create_evector(&ret->eventv,ret->vsizes)){
		free(ret);
		return NULL;
	}
	if(create_evector(&ret->changev,ret->vsizes)){
		destroy_evector(&ret->eventv);
		free(ret);
		return NULL;
	}
	ret->changesqueued = 0;
	return ret;
}

static void
destroy_evectors(evectors *e){
	if(e){
		destroy_evector(&e->changev);
		destroy_evector(&e->eventv);
		free(e);
	}
}

static int
add_evhandler_baseevents(evhandler *e){
	evectors *ev;
	sigset_t s;

	if((ev = create_evectors()) == NULL){
		return -1;
	}
	e->externalvec = ev;
	if(sigemptyset(&s) || sigaddset(&s,EVTHREAD_SIGNAL) || sigaddset(&s,EVTHREAD_TERM)){
		return -1;
	}
	if(add_signal_to_evhandler(e,&s,rxsignal,NULL)){
		e->externalvec = NULL;
		destroy_evectors(ev);
		return -1;
	}
	return 0;
}

static int
initialize_evhandler(evhandler *e,evtables *evsources,int fd){
	memset(e,0,sizeof(*e));
	if(pthread_mutex_init(&e->lock,NULL)){
		goto err;
	}
	e->evsources = evsources;
	e->efd = fd;
	if(add_evhandler_baseevents(e)){
		goto lockerr;
	}
	return 0;

lockerr:
	pthread_mutex_destroy(&e->lock);
err:
	return -1;
}

evhandler *create_evhandler(evtables *evsources){
	evhandler *ret;
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
		return NULL;
	}
	if(SAFE_EPOLL_CLOEXEC == 0){
		if(((flags = fcntl(fd,F_GETFD)) < 0) ||
				fcntl(fd,F_SETFD,flags | FD_CLOEXEC)){
			close(fd);
			return NULL;
		}
	}
#undef SAFE_EPOLL_CLOEXEC
#elif defined(LIBTORQUE_FREEBSD)
	if((fd = kqueue()) < 0){
		return NULL;
	}
	if(((flags = fcntl(fd,F_GETFD)) < 0) || fcntl(fd,F_SETFD,flags | FD_CLOEXEC)){
		close(fd);
		return NULL;
	}
#endif
	if( (ret = malloc(sizeof(*ret))) ){
		if(initialize_evhandler(ret,evsources,fd) == 0){
			return ret;
		}
		free(ret);
	}
	close(fd);
	return NULL;
}

static void print_evstats(const evthreadstats *stats){
#define PRINTSTAT(s,field) \
	do { if((s)->field){ printf(#field ": %ju\n",(s)->field); } }while(0)
#define STATDEF(field) PRINTSTAT(stats,field);
#include <libtorque/events/stats.h>
#undef STATDEF
#undef PRINTSTAT
}

int destroy_evhandler(evhandler *e){
	int ret = 0;

	if(e){
		print_evstats(&e->stats);
		ret |= pthread_mutex_destroy(&e->lock);
		destroy_evectors(e->externalvec);
		ret |= close(e->efd);
		free(e);
	}
	return ret;
}
