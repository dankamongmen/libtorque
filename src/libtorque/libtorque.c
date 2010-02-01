#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <libtorque/buffers.h>
#include <libtorque/internal.h>
#include <libtorque/libtorque.h>
#include <libtorque/events/fd.h>
#include <libtorque/protos/ssl.h>
#include <libtorque/protos/dns.h>
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
initialize_etables(torque_ctx *ctx,evtables *e,const sigset_t *ss){
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

#ifdef TORQUE_LINUX_SIGNALFD
	ret |= close(e->common_signalfd);
#endif
	ret |= destroy_evsources(e->sigarray);
	ret |= destroy_evsources(e->fdarray);
	return ret;
}

static inline torque_ctx *
create_torque_ctx(torque_err *e,const sigset_t *ss){
	torque_ctx *ret;

	if( (ret = malloc(sizeof(*ret))) ){
		if(initialize_etables(ret,&ret->eventtables,ss)){
			free(ret);
			*e = TORQUE_ERR_RESOURCE;
			return NULL;
		}
		if(init_evqueue(ret,&ret->evq)){
			free_etables(&ret->eventtables);
			free(ret);
			*e = TORQUE_ERR_RESOURCE;
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
free_torque_ctx(torque_ctx *ctx){
	int ret = 0;

	ret |= free_etables(&ctx->eventtables);
	free_architecture(ctx);
	ret |= destroy_evqueue(&ctx->evq);
	free(ctx);
	return ret;
}

static torque_ctx *
torque_init_sigmasked(torque_err *e,const sigset_t *ss){
	torque_ctx *ctx;

	if((ctx = create_torque_ctx(e,ss)) == NULL){
		return NULL;
	}
	if( (*e = detect_architecture(ctx)) ){
		free_torque_ctx(ctx);
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
#ifdef SIGSTKFLT
		SIGSTKFLT,
#endif
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
torque_err torque_sigmask(sigset_t *olds){
	sigset_t ss;

	if(sigemptyset(&ss) || sigaddset(&ss,EVTHREAD_TERM) ||
			sigaddset(&ss,EVTHREAD_INT)){
		return TORQUE_ERR_ASSERT;
	}
	if(pthread_sigmask(SIG_BLOCK,&ss,olds)){
		return TORQUE_ERR_ASSERT;
	}
	return 0;
}

torque_ctx *torque_init(torque_err *e){
	struct sigaction oldact;
	torque_ctx *ret;
	sigset_t old,add;

	*e = TORQUE_ERR_NONE;
	// If SIGPIPE isn't being handled or at least ignored, start ignoring
	// it (don't blow away a preexisting handler, though).
	if(sigaction(SIGPIPE,NULL,&oldact)){
		*e = TORQUE_ERR_ASSERT;
		return NULL;
	}
	if(oldact.sa_handler == SIG_DFL){
		oldact.sa_handler = SIG_IGN;
		if(sigaction(SIGPIPE,&oldact,NULL)){
			*e = TORQUE_ERR_ASSERT;
			return NULL;
		}
	}
	if(makesigmask(&add) || pthread_sigmask(SIG_BLOCK,&add,&old)){
		*e = TORQUE_ERR_ASSERT;
		return NULL;
	}
	ret = torque_init_sigmasked(e,&old);
	if(pthread_sigmask(SIG_SETMASK,&old,NULL)){
		torque_stop(ret);
		*e = TORQUE_ERR_ASSERT;
		return NULL;
	}
	return ret;
}

torque_err torque_addsignal(torque_ctx *ctx,const sigset_t *sigs,
			libtorquercb fxn,void *state){
	torque_err ret;
	sigset_t old;

	if(sigismember(sigs,EVTHREAD_TERM) || sigismember(sigs,SIGKILL) ||
			sigismember(sigs,SIGSTOP)){
		return TORQUE_ERR_INVAL;
	}
	if(pthread_sigmask(SIG_BLOCK,sigs,&old)){
		return TORQUE_ERR_ASSERT;
	}
	if( (ret = add_signal_to_evhandler(ctx,&ctx->evq,sigs,fxn,state)) ){
		pthread_sigmask(SIG_SETMASK,&old,NULL);
		return ret;
	}
	return 0;
}

torque_err torque_addtimer(torque_ctx *ctx,const struct itimerspec *t,
			libtorquetimecb fxn,void *state){
	return add_timer_to_evhandler(ctx,&ctx->evq,t,fxn,state);
}

// We only currently provide one buffering scheme. When that changes, we still
// won't want to expose anything more than necessary to applications...
torque_err torque_addfd(torque_ctx *ctx,int fd,libtorquebrcb rx,
				libtorquebwcb tx,void *state){
	torque_rxbufcb *cbctx;
	torque_err ret;

	if(fd < 0){
		return TORQUE_ERR_INVAL;
	}
	if((cbctx = create_rxbuffercb(ctx,rx,tx,state)) == NULL){
		return TORQUE_ERR_RESOURCE;
	}
	if( (ret = add_fd_to_evhandler(ctx,&ctx->evq,fd,buffered_rxfxn,buffered_txfxn,
					cbctx,EVONESHOT)) ){
		free_rxbuffercb(cbctx);
		return ret;
	}
	return 0;
}

torque_err torque_addfd_unbuffered(torque_ctx *ctx,int fd,libtorquercb rx,
				libtorquewcb tx,void *state){
	if(fd < 0){
		return TORQUE_ERR_INVAL;
	}
	return add_fd_to_evhandler(ctx,&ctx->evq,fd,rx,tx,state,EVONESHOT);
}

torque_err torque_addfd_concurrent(torque_ctx *ctx,int fd,
				libtorquercb rx,libtorquewcb tx,void *state){
	if(fd < 0){
		return TORQUE_ERR_INVAL;
	}
	return add_fd_to_evhandler(ctx,&ctx->evq,fd,rx,tx,state,0);
}

torque_err torque_addpath(torque_ctx *ctx,const char *path,libtorquercb rx,void *state){
	if(add_fswatch_to_evhandler(&ctx->evq,path,rx,state)){
		return TORQUE_ERR_UNAVAIL; // FIXME
	}
	return 0;
}

#ifndef LIBTORQUE_WITHOUT_SSL
torque_err torque_addssl(torque_ctx *ctx,int fd,SSL_CTX *sslctx,
			libtorquebrcb rx,libtorquebwcb tx,void *state){
	struct ssl_cbstate *cbs;

	if((cbs = create_ssl_cbstate(ctx,sslctx,state,rx,tx)) == NULL){
		return TORQUE_ERR_RESOURCE; // FIXME not necessarily correct
	}
	if(torque_addfd_unbuffered(ctx,fd,ssl_accept_rxfxn,NULL,cbs)){
		free_ssl_cbstate(cbs);
		return TORQUE_ERR_RESOURCE; // FIXME not necessarily correct
	}
	return 0;
}
#else
torque_err torque_addssl(torque_ctx *ctx __attribute__ ((unused)),
				int fd __attribute__ ((unused)),
				SSL_CTX *sslctx __attribute__ ((unused)),
				libtorquebrcb rx __attribute__ ((unused)),
				libtorquebwcb tx __attribute__ ((unused)),
				void *state __attribute__ ((unused))){
	return TORQUE_ERR_UNAVAIL;
}
#endif

#ifndef LIBTORQUE_WITHOUT_ADNS
torque_err torque_addlookup_dns(torque_ctx *ctx,const char *owner,
					libtorquednscb rx,void *state){
	struct dnsmarshal *dm;
	adns_query query;

	if((dm = create_dnsmarshal(rx,state)) == NULL){
		return TORQUE_ERR_RESOURCE;
	}
	// FIXME need to lock adns struct...should be one per evqueue
	// FIXME need allow other than A type!
	if(adns_submit(ctx->evq.dnsctx,owner,adns_r_a,adns_qf_none,dm,&query)){
		free_dnsmarshal(dm);
		return TORQUE_ERR_INVAL; // FIXME break down error cases
	}
	if(load_dns_fds(ctx,&ctx->evq.dnsctx,&ctx->evq)){
		adns_cancel(query);
		free_dnsmarshal(dm);
		return TORQUE_ERR_ASSERT; // FIXME break down error cases
	}
	return 0;
}
#else
torque_err torque_addlookup_dns(torque_ctx *ctx __attribute__ ((unused)),
				const char *owner __attribute__ ((unused)),
				libtorquednscb rx __attribute__ ((unused)),
				void *state __attribute__ ((unused))){
	return TORQUE_ERR_UNAVAIL;
}
#endif

// Performs a thread-local lookup of the current ctx. This must not be cached
// beyond the lifetime of the callback instance!
struct torque_ctx *torque_getcurctx(void){
	return get_thread_ctx();
}

torque_err torque_block(torque_ctx *ctx){
	int ret = 0;

	if(ctx){
		torque_err r;
		sigset_t os;

		// We ought have already have them blocked on entry, but...this
		// doesn't help if other threads are unblocked, though.
		if( (r = torque_sigmask(&os)) ){
			return r;
		}
		ret |= block_threads(ctx);
		ret |= free_torque_ctx(ctx);
		ret |= pthread_sigmask(SIG_SETMASK,&os,NULL);
	}
	return ret ? TORQUE_ERR_ASSERT : 0;
}

torque_err torque_stop(torque_ctx *ctx){
	int ret = 0;

	if(ctx){
		torque_err r;
		sigset_t os;

		// We ought have already have them blocked on entry, but...this
		// doesn't help if other threads are unblocked, though.
		if( (r = torque_sigmask(&os)) ){
			return r;
		}
		ret |= reap_threads(ctx);
		ret |= free_torque_ctx(ctx);
		ret |= pthread_sigmask(SIG_SETMASK,&os,NULL);
	}
	return ret ? TORQUE_ERR_ASSERT : 0;
}
