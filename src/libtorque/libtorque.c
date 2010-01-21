#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libtorque/buffers.h>
#include <libtorque/ssl/ssl.h>
#include <libtorque/internal.h>
#include <libtorque/libtorque.h>
#include <libtorque/events/fd.h>
#include <libtorque/events/evq.h>
#include <libtorque/events/path.h>
#include <libtorque/events/timer.h>
#include <libtorque/hardware/arch.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/signal.h>
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
initialize_etables(libtorque_ctx *ctx,evtables *e,const sigset_t *ss){
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
	if(initialize_common_sources(ctx,e,ss)){
		destroy_evsources(e->sigarray);
		destroy_evsources(e->fdarray);
		return -1;
	}
	return 0;
}

static int
free_etables(evtables *e){
	int ret = 0;

#ifdef LIBTORQUE_LINUX_SIGNALFD
	ret |= close(e->common_signalfd);
#endif
	ret |= destroy_evsources(e->sigarray);
	ret |= destroy_evsources(e->fdarray);
	return ret;
}

static inline libtorque_ctx *
create_libtorque_ctx(libtorque_err *e,const sigset_t *ss){
	libtorque_ctx *ret;

	if( (ret = malloc(sizeof(*ret))) ){
		if(initialize_etables(ret,&ret->eventtables,ss)){
			free(ret);
			*e = LIBTORQUE_ERR_RESOURCE;
			return NULL;
		}
		if(init_evqueue(ret,&ret->evq)){
			free_etables(&ret->eventtables);
			free(ret);
			*e = LIBTORQUE_ERR_RESOURCE;
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

static int
free_libtorque_ctx(libtorque_ctx *ctx){
	int ret = 0;

	ret |= free_etables(&ctx->eventtables);
	free_architecture(ctx);
	ret |= destroy_evqueue(&ctx->evq);
	free(ctx);
	return ret;
}

static libtorque_ctx *
libtorque_init_sigmasked(libtorque_err *e,const sigset_t *ss){
	libtorque_ctx *ctx;

	if((ctx = create_libtorque_ctx(e,ss)) == NULL){
		return NULL;
	}
	if( (*e = detect_architecture(ctx)) ){
		free_libtorque_ctx(ctx);
		return NULL;
	}
	return ctx;
}

// libtorque threads mask all signals save those synchronously delivered due to
// program error. those registered are unblocked in the event receipt loop, or
// handled as regular events via EVFILT_SIGNAL or signalfds.
static inline int
makesigmask(sigset_t *s){
	const int unblocked[] = {
		SIGILL,
		SIGABRT,
		SIGFPE,
		SIGSEGV,
		SIGBUS,
		SIGXCPU,
		SIGSTKFLT,
	};
	unsigned z;

	if(sigfillset(s)){
		return -1;
	}
	for(z = 0 ; z < sizeof(unblocked) / sizeof(*unblocked) ; ++z){
		if(sigdelset(s,unblocked[z])){
			return -1;
		}
	}
	return 0;
}

// Ought be called by the client application prior to launching any threads.
libtorque_err libtorque_sigmask(sigset_t *olds){
	sigset_t ss;

	if(sigemptyset(&ss) || sigaddset(&ss,EVTHREAD_TERM) ||
			sigaddset(&ss,EVTHREAD_INT)){
		return LIBTORQUE_ERR_ASSERT;
	}
	if(pthread_sigmask(SIG_BLOCK,&ss,olds)){
		return LIBTORQUE_ERR_ASSERT;
	}
	return 0;
}

libtorque_ctx *libtorque_init(libtorque_err *e,sigset_t *olds){
	struct sigaction oldact;
	libtorque_ctx *ret;
	sigset_t old,add;

	if(olds == NULL){
		olds = &old;
	}
	*e = LIBTORQUE_ERR_NONE;
	// If SIGPIPE isn't being handled or at least ignored, start ignoring
	// it (don't blow away a preexisting handler, though).
	if(sigaction(SIGPIPE,NULL,&oldact)){
		*e = LIBTORQUE_ERR_ASSERT;
		return NULL;
	}
	if(oldact.sa_handler == SIG_DFL){
		oldact.sa_handler = SIG_IGN;
		if(sigaction(SIGPIPE,&oldact,NULL)){
			*e = LIBTORQUE_ERR_ASSERT;
			return NULL;
		}
	}
	// Ensure that EVTHREAD_INT and EVTHREAD_TERM are both henceforth
	// blocked in the calling thread. This isn't full assurance (other
	// threads might have already been created with the signals unblocked),
	// but it's better than nothing.
	if(libtorque_sigmask(olds)){
		*e = LIBTORQUE_ERR_ASSERT;
		return NULL;
	}
	if(makesigmask(&add) || pthread_sigmask(SIG_BLOCK,&add,&old)){
		*e = LIBTORQUE_ERR_ASSERT;
		return NULL;
	}
	ret = libtorque_init_sigmasked(e,&old);
	// EVTHREAD_INT and _TERM were blocked before we got the old mask, so
	// they'll remain blocked on exit.
	if(pthread_sigmask(SIG_SETMASK,&old,NULL)){
		libtorque_stop(ret);
		*e = LIBTORQUE_ERR_ASSERT;
		return NULL;
	}
	return ret;
}

libtorque_err libtorque_addsignal(libtorque_ctx *ctx,const sigset_t *sigs,
			libtorquercb fxn,void *state){
	// FIXME check for empty signal set via sigisemptyset()/freebsd equiv
	// We do allow EVTHREAD_INT to be overriden -- that's just there for
	// convenience (possibly only mine). We *use* EVTHREAD_TERM, though.
	if(sigismember(sigs,EVTHREAD_TERM) || sigismember(sigs,SIGKILL) ||
			sigismember(sigs,SIGSTOP)){
		return LIBTORQUE_ERR_INVAL;
	}
	if(pthread_sigmask(SIG_BLOCK,sigs,NULL)){
		return LIBTORQUE_ERR_ASSERT;
	}
	if(add_signal_to_evhandler(ctx,&ctx->evq,sigs,fxn,state)){
		return LIBTORQUE_ERR_RESOURCE; // FIXME not necessarily correct
	}
	return 0;
}

libtorque_err libtorque_addtimer(libtorque_ctx *ctx,const struct itimerspec *t,
			libtorquercb fxn,void *state){
	if(add_timer_to_evhandler(ctx,&ctx->evq,t,fxn,state)){
		return LIBTORQUE_ERR_UNAVAIL; // FIXME
	}
	return 0;
}

// We only currently provide one buffering scheme. When that changes, we still
// won't want to expose anything more than necessary to applications...
libtorque_err libtorque_addfd(libtorque_ctx *ctx,int fd,libtorquebrcb rx,
				libtorquebwcb tx,void *state){
	libtorque_rxbufcb *cbctx;

	if(fd < 0){
		return LIBTORQUE_ERR_INVAL;
	}
	if((cbctx = create_rxbuffercb(ctx,rx,tx,state)) == NULL){
		return LIBTORQUE_ERR_RESOURCE;
	}
	if(add_fd_to_evhandler(ctx,&ctx->evq,fd,buffered_rxfxn,buffered_txfxn,
					cbctx,EPOLLONESHOT)){
		free_rxbuffercb(cbctx);
		return LIBTORQUE_ERR_RESOURCE; // FIXME not necessarily correct
	}
	return 0;
}

libtorque_err libtorque_addfd_unbuffered(libtorque_ctx *ctx,int fd,libtorquercb rx,
				libtorquewcb tx,void *state){
	if(fd < 0){
		return LIBTORQUE_ERR_INVAL;
	}
	if(add_fd_to_evhandler(ctx,&ctx->evq,fd,rx,tx,state,EPOLLONESHOT)){
		return LIBTORQUE_ERR_RESOURCE; // FIXME not necessarily correct
	}
	return 0;
}

libtorque_err libtorque_addfd_concurrent(libtorque_ctx *ctx,int fd,
				libtorquercb rx,libtorquewcb tx,void *state){
	if(fd < 0){
		return LIBTORQUE_ERR_INVAL;
	}
	if(add_fd_to_evhandler(ctx,&ctx->evq,fd,rx,tx,state,0)){
		return LIBTORQUE_ERR_RESOURCE; // FIXME not necessarily correct
	}
	return 0;
}

libtorque_err libtorque_addpath(libtorque_ctx *ctx,const char *path,libtorquercb rx,void *state){
	if(add_fswatch_to_evhandler(&ctx->evq,path,rx,state)){
		return LIBTORQUE_ERR_UNAVAIL; // FIXME
	}
	return 0;
}

#ifndef LIBTORQUE_WITHOUT_SSL
libtorque_err libtorque_addssl(libtorque_ctx *ctx,int fd,SSL_CTX *sslctx,
			libtorquebrcb rx,libtorquebwcb tx,void *state){
	struct ssl_cbstate *cbs;

	if((cbs = create_ssl_cbstate(ctx,sslctx,state,rx,tx)) == NULL){
		return LIBTORQUE_ERR_RESOURCE; // FIXME not necessarily correct
	}
	if(libtorque_addfd_unbuffered(ctx,fd,ssl_accept_rxfxn,NULL,cbs)){
		free_ssl_cbstate(cbs);
		return LIBTORQUE_ERR_RESOURCE; // FIXME not necessarily correct
	}
	return 0;
}
#else
libtorque_err libtorque_addssl(libtorque_ctx *ctx __attribute__ ((unused)),
				int fd __attribute__ ((unused)),
				SSL_CTX *sslctx __attribute__ ((unused)),
				libtorquebrcb rx __attribute__ ((unused)),
				libtorquebwcb tx __attribute__ ((unused)),
				void *state __attribute__ ((unused))){
	return LIBTORQUE_ERR_UNAVAIL;
}
#endif

#ifndef LIBTORQUE_WITHOUT_ADNS
libtorque_err libtorque_addlookup_dns(libtorque_ctx *ctx,const char *owner,
					libtorquercb rx,void *state){
	printf("%p %s %p %p\n",ctx,owner,rx,state); // FIXME
	return LIBTORQUE_ERR_UNAVAIL;
}
#else
libtorque_err libtorque_addlookup_dns(libtorque_ctx *ctx __attribute__ ((unused)),
				const char *owner __attribute__ ((unused)),
				libtorquercb rx __attribute__ ((unused)),
				void *state __attribute__ ((unused))){
	return LIBTORQUE_ERR_UNAVAIL;
}
#endif

// Performs a thread-local lookup of the current ctx. This must not be cached
// beyond the lifetime of the callback instance!
struct libtorque_ctx *libtorque_getcurctx(void){
	return get_thread_ctx();
}

libtorque_err libtorque_block(libtorque_ctx *ctx){
	int ret = 0;

	if(ctx){
		libtorque_err r;
		sigset_t os;

		// We ought have already have them blocked on entry, but...this
		// doesn't help if other threads are unblocked, though.
		if( (r = libtorque_sigmask(&os)) ){
			return r;
		}
		ret |= block_threads(ctx);
		ret |= free_libtorque_ctx(ctx);
		ret |= pthread_sigmask(SIG_SETMASK,&os,NULL);
	}
	return ret ? LIBTORQUE_ERR_ASSERT : 0;
}

libtorque_err libtorque_stop(libtorque_ctx *ctx){
	int ret = 0;

	if(ctx){
		ret |= reap_threads(ctx);
		ret |= free_libtorque_ctx(ctx);
	}
	return ret ? LIBTORQUE_ERR_ASSERT : 0;
}
