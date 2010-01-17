#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <bits/local_lim.h> // FIXME eglibc broke PTHREAD_STACK_MIN :/
#include <libtorque/internal.h>
#include <libtorque/events/evq.h>
#include <libtorque/events/thread.h>
#include <libtorque/events/sysdep.h>

// OpenSSL requires a numeric identifier for threads. On FreeBSD (using
// the default or libthr implementations), pthread_self() is insufficient; it
// seems to return an aggregate... :/
#ifdef LIBTORQUE_FREEBSD
static unsigned long openssl_id_idx;
static pthread_key_t openssl_id_key;
static pthread_once_t openssl_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t openssl_lock = PTHREAD_MUTEX_INITIALIZER;

static void
setup_openssl_idkey(void){
	if(pthread_key_create(&openssl_id_key,free)){
		// FIXME do what, exactly?
	}
}

static unsigned long *setup_new_sslid(void) __attribute__ ((malloc));

static unsigned long *
setup_new_sslid(void){
	unsigned long *ret;

	if( (ret = malloc(sizeof(*ret))) ){
		if(pthread_setspecific(openssl_id_key,ret) == 0){
			if(pthread_mutex_lock(&openssl_lock) == 0){
				*ret = ++openssl_id_idx;
				return ret;
			}
			pthread_setspecific(openssl_id_key,NULL);
		}
		free(ret);
	}
	return NULL;
}

unsigned long pthread_self_getnumeric(void){
	unsigned long *key;

	pthread_once(&openssl_once,setup_openssl_idkey);
	if((key = pthread_getspecific(openssl_id_key)) == NULL){
		key = setup_new_sslid(); // what if this fails? FIXME
	}
	return *key;
}
#endif

// Pins the current thread to the given cpuset ID, ie [0..cpuset_size()).
int pin_thread(unsigned aid){
	cpu_set_t mask;

	CPU_ZERO(&mask);
	CPU_SET(aid,&mask);
#ifdef LIBTORQUE_FREEBSD
	if(cpuset_setaffinity(CPU_LEVEL_WHICH,CPU_WHICH_TID,-1,
				sizeof(mask),&mask)){
#else
	if(sched_setaffinity(0,sizeof(mask),&mask)){
#endif
		return -1;
	}
	return 0;
}

static int
block_thread(pthread_t tid){
	void *joinret = PTHREAD_CANCELED;

	// FIXME various race conditions cause NULL to emerge from
	// pthread_join(). they're not important, save being logically tidy
	// (and worth investigating)...for now, toss that check.
	if(pthread_join(tid,&joinret)/* || joinret != PTHREAD_CANCELED*/){
		return -1;
	}
	return 0;
}

static inline int
reap_thread(pthread_t tid){
	sigset_t ss,os;
	int ret = 0;

	// No need to watch out for EVTHREAD_INT here (the issue is that
	// EVTHREAD_TERM will be raised freely in a chain reaction).
	if(sigemptyset(&ss) || sigaddset(&ss,EVTHREAD_TERM)){
		return -1;
	}
	if(pthread_sigmask(SIG_BLOCK,&ss,&os)){
		return -1;
	}
	// POSIX has a special case for process-autodirected signals; kill(2)
	// does not return until at least one signal is delivered. This works
	// around a race condition (see block_thread()) *most* of the time.
	// There's two problems with it: should some other thread have
	// EVTHREAD_TERM unblocked, this breaks (but they shouldn't be doing
	// that anyway), and should some other signal already be pending, it
	// breaks (we're only guaranteed that *one* signal gets delivered
	// before we return, so we can still hit the block_thread() early).
	ret |= kill(getpid(),EVTHREAD_TERM);
	ret |= block_thread(tid);
	ret |= pthread_sigmask(SIG_SETMASK,&os,NULL);
	return ret;
}

typedef struct tguard {
	libtorque_ctx *ctx;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	stack_t stack;
	enum {
		THREAD_UNLAUNCHED = 0,
		THREAD_PREFAIL,
		THREAD_STARTED,
	} status;
} tguard;

// On entry, cancellation ought be disabled, and execution restricted to a
// single processor via hard affinity settings (FIXME: verify?).
static void *
thread(void *void_marshal){
	tguard *marshal = void_marshal;
	evhandler *ev = NULL;
	libtorque_ctx *ctx;

	ctx = marshal->ctx;
	if(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL)){
		goto earlyerr;
	}
	if((ev = create_evhandler(&ctx->evq,&marshal->stack)) == NULL){
		goto earlyerr;
	}
	if(pthread_mutex_lock(&marshal->lock)){
		destroy_evhandler(ev);
		goto earlyerr;
	}
	if(marshal->ctx->ev == NULL){
		marshal->ctx->ev = ev;
	}else{
		ev->nexttid = marshal->ctx->ev->nexttid;
	}
	marshal->ctx->ev->nexttid = pthread_self();
	marshal->status = THREAD_STARTED;
	pthread_cond_broadcast(&marshal->cond);
	pthread_mutex_unlock(&marshal->lock);
	// After this point, anything we wish to use from marshal must've been
	// copied onto our own stack (hence broadcasting prior to unlocking).
	event_thread(ctx,ev);
	destroy_evhandler(ev);
	return NULL;

earlyerr:
	pthread_mutex_lock(&marshal->lock); // continue regardless
	marshal->status = THREAD_PREFAIL;
	pthread_cond_broadcast(&marshal->cond);
	pthread_mutex_unlock(&marshal->lock);
	destroy_evhandler(ev);
	return NULL;
}

static inline
int setup_thread_stack(stack_t *s,pthread_attr_t *attr){
	if(pthread_attr_init(attr)){
		return -1;
	}
	s->ss_flags = 0;
	// FIXME might want a larger stack? definitely consider caches. also
	/* need align stack on page boundary (see pthread_attr_setstack(3))
	s->ss_size = PTHREAD_STACK_MIN >= SIGSTKSZ ?
		PTHREAD_STACK_MIN : SIGSTKSZ;
	if((s->ss_sp = malloc(s->ss_size)) == NULL){
		pthread_attr_destroy(&attr);
		return -1;
	}
	if(pthread_attr_setstack(attr,s->ss_sp,s->ss_size)){
		free(s->ss_sp);
		pthread_attr_destroy(&attr);
		return -1;
	}*/
	s->ss_size = 0;
	s->ss_sp = NULL;
	return 0;
}

// Must be pinned to the desired CPU upon entry! // FIXME verify?
int spawn_thread(libtorque_ctx *ctx){
	pthread_attr_t attr;
	tguard tidguard = {
		.ctx = ctx,
	};
	pthread_t tid;
	int ret = 0;

	if(setup_thread_stack(&tidguard.stack,&attr)){
		return -1;
	}
	if(pthread_mutex_init(&tidguard.lock,NULL)){
		free(tidguard.stack.ss_sp);
		pthread_attr_destroy(&attr);
		return -1;
	}
	if(pthread_cond_init(&tidguard.cond,NULL)){
		pthread_mutex_destroy(&tidguard.lock);
		free(tidguard.stack.ss_sp);
		pthread_attr_destroy(&attr);
		return -1;
	}
	if(pthread_create(&tid,&attr,thread,&tidguard)){
		pthread_mutex_destroy(&tidguard.lock);
		pthread_cond_destroy(&tidguard.cond);
		free(tidguard.stack.ss_sp);
		pthread_attr_destroy(&attr);
		return -1;
	}
	ret |= pthread_attr_destroy(&attr);
	ret |= pthread_mutex_lock(&tidguard.lock);
	while(tidguard.status == THREAD_UNLAUNCHED){
		ret |= pthread_cond_wait(&tidguard.cond,&tidguard.lock);
	}
	if(tidguard.status != THREAD_STARTED){
		ret = -1;
	}
	ret |= pthread_mutex_unlock(&tidguard.lock);
	ret |= pthread_cond_destroy(&tidguard.cond);
	ret |= pthread_mutex_destroy(&tidguard.lock);
	if(ret){
		pthread_join(tid,NULL);
	}
	return ret;
}

int reap_threads(libtorque_ctx *ctx){
	int ret = 0;

	if(ctx->ev){
		ret = reap_thread(ctx->ev->nexttid);
	}
	return ret;
}

int block_threads(libtorque_ctx *ctx){
	int ret = 0;

	if(ctx->ev){
		ret = block_thread(ctx->ev->nexttid);
	}
	return ret;
}

// This function can be very slow unless we have a getcpu() system call (for
// instance on Linux with glibc 2.6, see below). It can also suffer an
// exception in that case, should the affinity mask not be a single CPU.
int get_thread_aid(void){
	// sched_getcpu() is unsafe on ubuntu 8.04's glibc 2.7 + 2.6.9; it
	// coredumps on entry, jumping to an invalid address. best avoided.
	// this might actually be related to valgrind. hrmm.
#if defined(LIBTORQUE_LINUX) && (__GLIBC__ > 2 || __GLIBC__ == 2 && __GLIBC_MINOR__ > 7)
	return sched_getcpu();
#else
	cpu_set_t mask;
	int z;

#ifdef LIBTORQUE_FREEBSD
	if(cpuset_getaffinity(CPU_LEVEL_WHICH,CPU_WHICH_TID,-1,
				sizeof(mask),&mask)){
#else
	if(sched_getaffinity(0,sizeof(mask),&mask)){
#endif
		return -1;
	}
	for(z = 0 ; z < CPU_SETSIZE ; ++z){
		if(CPU_ISSET(z,&mask)){
			return z;
		}
	}
	return -1;
#endif
}
