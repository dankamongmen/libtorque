#include <unistd.h>
#include <libtorque/events/evq.h>
#include <libtorque/events/thread.h>

int initialize_evq(libtorque_evq *evq){
	if(pthread_mutex_init(&evq->qlock,NULL) == 0){
		if((evq->efd = create_efd()) >= 0){
			return 0;
		}
		pthread_mutex_destroy(&evq->qlock);
	}
	return -1;
}

int destroy_evq(libtorque_evq *evq){
	int ret = 0;

	ret |= pthread_mutex_destroy(&evq->qlock);
	ret |= close(evq->efd);
	return ret;
}
