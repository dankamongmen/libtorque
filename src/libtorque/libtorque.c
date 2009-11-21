#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libtorque/ssl/ssl.h>
#include <libtorque/internal.h>
#include <libtorque/libtorque.h>
#include <libtorque/events/fds.h>
#include <libtorque/hardware/arch.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/signals.h>
#include <libtorque/events/sources.h>

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

static inline int
initialize_etables(evtables *e){
	if((e->fdarraysize = max_fds()) <= 0){
		return -1;
	}
	if((e->fdarray = create_evsources(e->fdarraysize)) == NULL){
		return -1;
	}
	// Need we really go all the way through SIGRTMAX? FreeBSD 6 doesn't
	// even define it argh! FIXME
#ifdef SIGRTMAX
	e->sigarraysize = SIGRTMAX;
#else
	e->sigarraysize = SIGUSR2;
#endif
	if((e->sigarray = create_evsources(e->sigarraysize)) == NULL){
		destroy_evsources(e->fdarray);
		return -1;
	}
	return 0;
}

static inline libtorque_ctx *
create_libtorque_ctx(void){
	libtorque_ctx *ret;

	if( (ret = malloc(sizeof(*ret))) ){
		if(initialize_etables(&ret->eventtables)){
			free(ret);
			return NULL;
		}
		ret->sched_zone = NULL;
		ret->cpudescs = NULL;
		ret->manodes = NULL;
		ret->cpu_typecount = 0;
		ret->nodecount = 0;
		ret->ev = NULL;
	}
	return ret;
}

static void
free_libtorque_ctx(libtorque_ctx *ctx){
	destroy_evsources(ctx->eventtables.sigarray);
	destroy_evsources(ctx->eventtables.fdarray);
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

	if(sigemptyset(&add) || sigaddset(&add,EVTHREAD_SIGNAL)
			|| sigaddset(&add,EVTHREAD_TERM)){
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

int libtorque_addsignal(libtorque_ctx *ctx,const sigset_t *sigs,
			libtorquercb fxn,void *state){
	if(add_signal_to_evhandler(ctx->ev,sigs,fxn,state)){
		return -1;
	}
	ctx->ev = ctx->ev->nextev;
	return 0;
}

int libtorque_addfd(libtorque_ctx *ctx,int fd,libtorquercb rx,
				libtorquewcb tx,void *state){
	if(fd < 0){
		return -1;
	}
	if(add_fd_to_evhandler(ctx->ev,fd,rx,tx,state)){
		return -1;
	}
	ctx->ev = ctx->ev->nextev;
	return 0;
}

#ifndef LIBTORQUE_WITHOUT_SSL
int libtorque_addssl(libtorque_ctx *ctx,int fd,SSL_CTX *sslctx,
			libtorquercb rx,libtorquewcb tx,void *state){
	struct ssl_cbstate *cbs;

	if((cbs = create_ssl_cbstate(ctx,sslctx,state,rx,tx)) == NULL){
		return -1;
	}
	if(libtorque_addfd(ctx,fd,ssl_accept_rxfxn,NULL,cbs)){
		free_ssl_cbstate(cbs);
		return -1;
	}
	return 0;
}
#endif

int libtorque_stop(libtorque_ctx *ctx){
	int ret = 0;

	if(ctx){
		ret |= reap_threads(ctx);
		free_libtorque_ctx(ctx);
	}
	return 0;
}
