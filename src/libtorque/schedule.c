#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <libtorque/internal.h>
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
	if(pthread_setaffinity_np(pthread_self(),sizeof(mask),&mask)){
#endif
		return -1;
	}
	return 0;
}

static int
reap_thread(pthread_t tid){
	void *joinret;
	int ret = 0;

	// If a thread has exited but not yet been joined, it's safe to call
	// pthread_cancel(). I don't think the same applies here; ignore error.
	pthread_kill(tid,EVTHREAD_TERM);
	if(pthread_join(tid,&joinret) || joinret != PTHREAD_CANCELED){
		ret = -1;
	}
	return 0;
}

typedef struct tguard {
	libtorque_ctx *ctx;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	enum {
		THREAD_UNLAUNCHED = 0,
		THREAD_PREFAIL,
		THREAD_STARTED,
	} status;
} tguard;

// Ought be restricted to a single processor on entry! // FIXME verify?
static void *
thread(void *void_marshal){
	tguard *marshal = void_marshal;
	libtorque_ctx *ctx = NULL;
	evhandler *ev = NULL;

	if(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL)){
		goto earlyerr;
	}
	if((ev = create_evhandler(&marshal->ctx->eventtables)) == NULL){
		goto earlyerr;
	}
	if(pthread_mutex_lock(&marshal->lock)){
		destroy_evhandler(ev);
		goto earlyerr;
	}
	if(marshal->ctx->ev == NULL){
		ev->nextev = ev;
		marshal->ctx->ev = ev;
		ev->nexttid = pthread_self();
	}else{
		ev->nextev = marshal->ctx->ev->nextev;
		marshal->ctx->ev->nextev = ev;
		ev->nexttid = marshal->ctx->ev->nexttid;
		marshal->ctx->ev->nexttid = pthread_self();
	}
	ctx = marshal->ctx;
	marshal->status = THREAD_STARTED;
	pthread_cond_broadcast(&marshal->cond);
	pthread_mutex_unlock(&marshal->lock);
	// After this point, anything we wish to use from tguard must have been
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

// Must be pinned to the desired CPU upon entry! // FIXME verify?
int spawn_thread(libtorque_ctx *ctx){
	tguard tidguard = {
		.ctx = ctx,
	};
	pthread_t tid;
	int ret = 0;

	if(pthread_mutex_init(&tidguard.lock,NULL)){
		return -1;
	}
	if(pthread_cond_init(&tidguard.cond,NULL)){
		pthread_mutex_destroy(&tidguard.lock);
		return -1;
	}
	if(pthread_create(&tid,NULL,thread,&tidguard)){
		pthread_mutex_destroy(&tidguard.lock);
		pthread_cond_destroy(&tidguard.cond);
		return -1;
	}
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
