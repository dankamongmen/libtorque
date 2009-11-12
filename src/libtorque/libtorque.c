#include <stdlib.h>
#include <string.h>
#include <libtorque/ssl/ssl.h>
#include <libtorque/internal.h>
#include <libtorque/libtorque.h>
#include <libtorque/events/fds.h>
#include <libtorque/hardware/arch.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/signals.h>
#include <libtorque/events/sources.h>

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

static libtorque_ctx *
libtorque_init_sigmasked(void){
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

libtorque_ctx *libtorque_init(void){
	libtorque_ctx *ret;
	sigset_t old,add;

	if(sigemptyset(&add) || sigaddset(&add,EVTHREAD_SIGNAL)){
		return NULL;
	}
	if(pthread_sigmask(SIG_BLOCK,&add,&old)){
		return NULL;
	}
	ret = libtorque_init_sigmasked();
	if(pthread_sigmask(SIG_SETMASK,&old,NULL)){
		libtorque_stop(ret);
		return NULL;
	}
	return ret;
}

int libtorque_addsignal(libtorque_ctx *ctx,int sig,libtorque_evcbfxn fxn,
					void *state){
	if(sig <= 0){
		return -1;
	}
	return add_signal_to_evhandler(ctx->ev,sig,fxn,state);
}

int libtorque_addfd(libtorque_ctx *ctx,int fd,libtorque_evcbfxn rx,
				libtorque_evcbfxn tx,void *state){
	if(fd < 0){
		return -1;
	}
	return add_fd_to_evhandler(ctx->ev,fd,rx,tx,state);
}

int libtorque_addssl(libtorque_ctx *ctx,int fd,SSL_CTX *sslctx,
			libtorque_evcbfxn rx,libtorque_evcbfxn tx,void *state){
	struct ssl_accept_cbstate *cbs;

	if((cbs = create_ssl_accept_cbstate(sslctx,state,rx,tx)) == NULL){
		return -1;
	}
	if(libtorque_addfd(ctx,fd,ssl_accept_rxfxn,NULL,cbs)){
		free_ssl_accept_cbstate(cbs);
		return -1;
	}
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
