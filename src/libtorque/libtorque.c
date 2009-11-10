#include <stdlib.h>
#include <libtorque/internal.h>
#include <libtorque/schedule.h>
#include <libtorque/libtorque.h>
#include <libtorque/hardware/arch.h>

static inline libtorque_ctx *
create_libtorque_ctx(void){
	libtorque_ctx *ret;

	if( (ret = malloc(sizeof(*ret))) ){
		ret->sched_zone = NULL;
	}
	return ret;
}

static void
free_libtorque_ctx(libtorque_ctx *ctx){
	free_architecture(ctx);
	free(ctx);
}

libtorque_ctx *libtorque_init(void){
	libtorque_ctx *ctx;

	if((ctx = create_libtorque_ctx()) == NULL){
		return NULL;
	}
	if(detect_architecture(ctx)){
		free_libtorque_ctx(ctx);
		return NULL;
	}
	return ctx;
}

int libtorque_spawn(void){
	return spawn_threads();
}

int libtorque_reap(void){
	return reap_threads();
}

int libtorque_stop(libtorque_ctx *ctx){
	int ret = 0;

	if(ctx){
		ret |= reap_threads();
		free_libtorque_ctx(ctx);
	}
	return 0;
}
