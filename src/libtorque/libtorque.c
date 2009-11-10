#include <stdlib.h>
#include <string.h>
#include <libtorque/internal.h>
#include <libtorque/libtorque.h>
#include <libtorque/hardware/arch.h>

static inline libtorque_ctx *
create_libtorque_ctx(void){
	libtorque_ctx *ret;

	if( (ret = malloc(sizeof(*ret))) ){
		memset(&ret->cpu_map,0,sizeof(ret->cpu_map));
		memset(&ret->affinmap,0,sizeof(ret->affinmap));
		CPU_ZERO(&ret->validmap);
		ret->sched_zone = NULL;
		ret->cpudescs = NULL;
		ret->manodes = NULL;
		ret->cpu_typecount = 0;
		ret->nodecount = 0;
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

int libtorque_spawn(libtorque_ctx *ctx){
	return spawn_threads(ctx);
}

int libtorque_stop(libtorque_ctx *ctx){
	int ret = 0;

	if(ctx){
		ret |= reap_threads(ctx,ctx->cpucount);
		free_libtorque_ctx(ctx);
	}
	return 0;
}
