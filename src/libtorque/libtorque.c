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

int libtorque_addsignal(libtorque_ctx *ctx,int sig,libtorque_evcbfxn fxn,
					void *state){
	if(sig <= 0){
		return -1;
	}
	if(ctx == NULL || fxn == NULL){
		return -1;
	}
	if(state == NULL){
		return -1;
	} // FIXME
	return 0;
}

int libtorque_addfd(libtorque_ctx *ctx,int fd,libtorque_evcbfxn rx,
				libtorque_evcbfxn tx,void *state){
	if(fd <= 0){
		return -1;
	}
	if(ctx == NULL || (rx == NULL && tx == NULL)){
		return -1;
	}
	if(state == NULL){
		return -1;
	} // FIXME
	return 0;
}

int libtorque_addssl(libtorque_ctx *ctx,int fd,SSL_CTX *sslctx,
			libtorque_evcbfxn rx,libtorque_evcbfxn tx,void *state){
	if(fd <= 0){
		return -1;
	}
	if(ctx == NULL || (rx == NULL && tx == NULL)){
		return -1;
	}
	if(state == NULL || sslctx == NULL){
		return -1;
	} // FIXME
	return 0;
}

int libtorque_stop(libtorque_ctx *ctx){
	int ret = 0;

	if(ctx){
		ret |= reap_threads(ctx,ctx->cpucount);
		free_libtorque_ctx(ctx);
	}
	return 0;
}
