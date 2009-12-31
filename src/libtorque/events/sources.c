#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/sources.h>

evsource *create_evsources(unsigned n){
	evsource *evs;

	if( (evs = malloc(sizeof(*evs) * n)) ){
		unsigned z;

		memset(evs,0,sizeof(*evs) * n);
		for(z = 0 ; z < n ; ++z){
			if(pthread_mutex_init(&evs->sboard.lock,NULL)){
				while(z--){
					pthread_mutex_destroy(&evs->sboard.lock);
				}
				free(evs);
				return NULL;
			}
		}
	}
	return evs;
}

int destroy_evsources(evsource *evs){
	int ret = 0;

	if(evs){
		// FIXME need to pthread_mutex_destroy() the locks
		free(evs);
	}
	return ret;
}
