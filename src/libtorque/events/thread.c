#include <unistd.h>
#include <pthread.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/sources.h>

#define EVTHREAD_SIGNAL SIGURG

// We're currently cancellable. That isn't generally safe unless we wrap
// handling with cancellation blocking, which eats a bit of performance. We'd
// like to encode cancellation into event handling itself FIXME.
void event_thread(evhandler *e){
	int newcstate = PTHREAD_CANCEL_DISABLE;

	while(1){
		int events,oldcstate;
		evectors ev = {
			.vsizes = 0,
			.changesqueued = 0,
		}; // FIXME

		events = Kevent(e->efd,PTR_TO_CHANGEV(&ev),ev.changesqueued,
				PTR_TO_EVENTV(&ev),ev.vsizes);
		pthread_setcancelstate(newcstate,&oldcstate);
		pthread_setcancelstate(oldcstate,&newcstate);
		pthread_testcancel();
	}
}

/*static int
add_evhandler_baseevents(evhandler *e){
	evectors *ev;

	if((ev = create_evectors()) == NULL){
		return -1;
	}
	if(add_signal_to_evcore(e,ev,EVTHREAD_SIGNAL,NULL,NULL)){
		destroy_evectors(ev);
		return -1;
	}
#ifdef LIBTORQUE_LINUX
	if(Kevent(e->fd,&ev->changev,ev->changesqueued,NULL,0,NULL)){
#else
	if(Kevent(e->fd,ev->changev,ev->changesqueued,NULL,0,NULL)){
#endif
		destroy_evectors(ev);
		return -1;
	}
	ev->changesqueued = 0;
	e->externalvec = ev;
	return 0;
}*/

static int
initialize_evhandler(evhandler *e,int fd){
	if(pthread_mutex_init(&e->lock,NULL)){
		goto err;
	}
	/*if(pthread_cond_init(&e->cond,NULL)){
		goto lockerr;
	}*/
	e->efd = fd;
	/*e->fdarraysize = determine_max_fds();
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
	}*/
	return 0;
/*
sigerr:
	destroy_evsources(e->sigarray,e->sigarraysize);
fderr:
	destroy_evsources(e->fdarray,e->fdarraysize);
conderr:
	pthread_cond_destroy(&e->cond);
lockerr:
	pthread_mutex_destroy(&e->lock);*/
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
#else
#error "No OS support for event queues"
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

static void
#ifdef LIBTORQUE_LINUX
destroy_evector(struct kevent *kv){
	free(kv->events);
	free(kv->ctldata);
#elif defined(LIBTORQUE_FREEBSD)
destroy_evector(struct kevent **kv){
	free(*kv);
#endif
}

static void
destroy_evectors(evectors *e){
        if(e){
		destroy_evector(&e->changev);
		destroy_evector(&e->eventv);
		free(e);
	}
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
