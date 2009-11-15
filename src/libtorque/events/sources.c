#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/sources.h>

evsource *create_evsources(unsigned n){
	evsource *evs;

	if( (evs = malloc(sizeof(*evs) * n)) ){
		memset(evs,0,sizeof(*evs) * n);
	}
	return evs;
}

int destroy_evsources(evsource *evs,unsigned n __attribute__ ((unused))){
	int ret = 0;

	if(evs){
		free(evs);
	}
	return ret;
}
