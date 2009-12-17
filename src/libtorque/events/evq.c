#include <unistd.h>
#include <libtorque/events/evq.h>
#include <libtorque/events/thread.h>

int destroy_evqueue(evqueue *evq){
	int ret = 0;

	if(pthread_mutex_lock(&evq->lock)){
		return -1;
	}
	if(--evq->refcount == 0){
		ret |= close(evq->efd);
		evq->efd = -1;
	}
	ret |= pthread_mutex_unlock(&evq->lock);
	return ret;
}

int ref_evqueue(evqueue *e){
	int ret = 0;

	if(pthread_mutex_lock(&e->lock)){
		return -1;
	}
	++e->refcount;
	ret |= pthread_mutex_unlock(&e->lock);
	return ret;
}

int init_evqueue(evqueue *e){
	if(pthread_mutex_init(&e->lock,NULL)){
		return -1;
	}
	if((e->efd = create_efd()) < 0){
		pthread_mutex_destroy(&e->lock);
		return -1;
	}
	e->refcount = 1;
	return 0;
}
