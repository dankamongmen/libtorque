#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/signals.h>
#include <libtorque/events/sources.h>

static void
handle_event(evhandler *eh,const kevententry *e){
	int ret = 0;

	if(e->events & EPOLLIN){
		ret |= handle_evsource_read(eh->fdarray,e->data.fd);
	}
	if(e->events & EPOLLOUT){
		ret |= handle_evsource_write(eh->fdarray,e->data.fd);
	}
	printf("result code: %d\n",ret);
}

void event_thread(evhandler *e){
	while(1){
		evectors *ev = e->externalvec; // FIXME really? surely not.
		int events,z;

		events = Kevent(e->efd,PTR_TO_CHANGEV(ev),ev->changesqueued,
				PTR_TO_EVENTV(ev),ev->vsizes);
		for(z = 0 ; z < events ; ++z){
			handle_event(e,&PTR_TO_EVENTV(ev)->events[z]);
		}
	}
}

static unsigned long
max_fds(void){
	long sc;

	if((sc = sysconf(_SC_OPEN_MAX)) <= 0){
		if((sc = getdtablesize()) <= 0){ // just checks rlimit
			sc = FOPEN_MAX; // ugh
		}
	}
	return sc;
}

#ifdef LIBTORQUE_LINUX
static int
create_evector(struct kevent *kv){
	if((kv->events = malloc(max_fds() * sizeof(*kv->events))) == NULL){
		return -1;
	}
	if((kv->ctldata = malloc(max_fds() * sizeof(*kv->ctldata))) == NULL){
		free(kv->events);
		return -1;
	}
	return 0;
}
#endif

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
	int maxfds = max_fds();
	evectors *ret;


	if((ret = malloc(sizeof(*ret))) == NULL){
		return NULL;
	}
#ifdef LIBTORQUE_LINUX
	if(create_evector(&ret->eventv)){
		free(ret);
		return NULL;
	}
	if(create_evector(&ret->changev)){
		destroy_evector(&ret->eventv);
		free(ret);
		return NULL;
	}
#else
	ret->eventv = malloc(maxfds * sizeof(*ret->eventv));
	ret->changev = malloc(maxfds * sizeof(*ret->changev));
	if(!ret->eventv || !ret->changev){
		free(ret->changev);
		free(ret->eventv);
		free(ret);
		return NULL;
	}
#endif
	ret->changesqueued = 0;
	ret->vsizes = maxfds;
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
rxsignal(int sig,void *unsafe_e){
	evhandler *e = unsafe_e;

	if(sig == EVTHREAD_TERM){
		destroy_evhandler(e);
		pthread_exit(PTHREAD_CANCELED);
	}
	return 0;
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
	if(add_signal_to_evhandler(e,&s,rxsignal,e)){
		e->externalvec = NULL;
		destroy_evectors(ev);
		return -1;
	}
	return 0;
}

static int
initialize_evhandler(evhandler *e,int fd){
	if(pthread_mutex_init(&e->lock,NULL)){
		goto err;
	}
	e->efd = fd;
	e->fdarraysize = max_fds();
	if((e->fdarray = create_evsources(e->fdarraysize)) == NULL){
		goto lockerr;
	}
	// Need we really go all the way through SIGRTMAX? FreeBSD 6 doesn't
	// even define it argh! FIXME
#ifdef SIGRTMAX
	e->sigarraysize = SIGRTMAX;
#else
	e->sigarraysize = SIGUSR2;
#endif
	if((e->sigarray = create_evsources(e->sigarraysize)) == NULL){
		goto fderr;
	}
	if(add_evhandler_baseevents(e)){
		goto sigerr;
	}
	return 0;

sigerr:
	destroy_evsources(e->sigarray,e->sigarraysize);
	
fderr:
	destroy_evsources(e->fdarray,e->fdarraysize);
lockerr:
	pthread_mutex_destroy(&e->lock);
err:
	return -1;
}

evhandler *create_evhandler(void){
	evhandler *ret;
	int fd;

// Until the epoll API stabilizes a bit... :/
#ifdef LIBTORQUE_LINUX
#ifdef EPOLL_CLOEXEC
#define SAFE_EPOLL_CLOEXEC EPOLL_CLOEXEC
#else // otherwise, it wants a size hint in terms of fd's
#include <linux/limits.h>
#define SAFE_EPOLL_CLOEXEC NR_OPEN
#endif
	if((fd = epoll_create(SAFE_EPOLL_CLOEXEC)) < 0){
#undef SAFE_EPOLL_CLOEXEC
		return NULL;
	}
#elif defined(LIBTORQUE_FREEBSD)
	if((fd = kqueue()) < 0){
		return NULL;
	}
	if(set_fd_close_on_exec(fd)){
		close(fd);
		return NULL;
	}
	if(set_fd_nonblocking(fd)){
		close(fd);
		return NULL;
	}
#endif
	if( (ret = malloc(sizeof(*ret))) ){
		memset(ret,0,sizeof(*ret));
		if(initialize_evhandler(ret,fd) == 0){
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
	PRINTSTAT(stats,evhandler_errors);
#undef PRINTSTAT
}

int destroy_evhandler(evhandler *e){
	int ret = 0;

	if(e){
		print_evstats(&e->stats);
		ret |= pthread_mutex_destroy(&e->lock);
		ret |= destroy_evsources(e->sigarray,e->sigarraysize);
		ret |= destroy_evsources(e->fdarray,e->fdarraysize);
		destroy_evectors(e->externalvec);
		ret |= close(e->efd);
		free(e);
	}
	return ret;
}
