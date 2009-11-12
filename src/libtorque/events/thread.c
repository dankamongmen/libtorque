#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/signals.h>
#include <libtorque/events/sources.h>

static void
handle_event(const kevententry *e){
	if(e->data.fd >= 0){
		printf("handling %d\n",e->data.fd);
	}
}

// We're currently cancellable. That isn't generally safe unless we wrap
// handling with cancellation blocking, which eats a bit of performance. We'd
// like to encode cancellation into event handling itself FIXME.
void event_thread(evhandler *e){
	int newcstate = PTHREAD_CANCEL_DISABLE;

	while(1){
		evectors *ev = e->externalvec; // FIXME really?
		int events,oldcstate,z;

		events = Kevent(e->efd,PTR_TO_CHANGEV(ev),ev->changesqueued,
				PTR_TO_EVENTV(ev),ev->vsizes);
		for(z = 0 ; z < events ; ++z){
			handle_event(&PTR_TO_EVENTV(ev)->events[z]);
		}
		pthread_setcancelstate(newcstate,&oldcstate);
		pthread_setcancelstate(oldcstate,&newcstate);
		pthread_testcancel();
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
add_evhandler_baseevents(evhandler *e){
	evectors *ev;

	if((ev = create_evectors()) == NULL){
		return -1;
	}
	e->externalvec = ev;
	if(add_signal_to_evhandler(e,EVTHREAD_SIGNAL,NULL,NULL)){
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
	/*if(pthread_cond_init(&e->cond,NULL)){
		goto lockerr;
	}*/
	e->efd = fd;
	e->fdarraysize = max_fds();
	if((e->fdarray = create_evsources(e->fdarraysize)) == NULL){
		goto conderr;
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
conderr:
	//pthread_cond_destroy(&e->cond);
//lockerr:
	pthread_mutex_destroy(&e->lock);
err:
	return -1;
}

evhandler *create_evhandler(void){
	evhandler *ret;
	int fd;

#ifdef LIBTORQUE_LINUX
	if((fd = epoll_create1(EPOLL_CLOEXEC)) < 0){
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
		if(initialize_evhandler(ret,fd) == 0){
			return ret;
		}
		free(ret);
	}
	close(fd);
	return NULL;
}

int destroy_evhandler(evhandler *e){
	int ret = 0;

	if(e){
		ret |= pthread_mutex_destroy(&e->lock);
		//ret |= pthread_cond_destroy(&e->cond);
		ret |= destroy_evsources(e->sigarray,e->sigarraysize);
		ret |= destroy_evsources(e->fdarray,e->fdarraysize);
		destroy_evectors(e->externalvec);
		ret |= close(e->efd);
		free(e);
	}
	return ret;
}
