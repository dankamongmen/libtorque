#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <libtorque/internal.h>
#include <libtorque/events/thread.h>

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
	if((key = pthread_getspecific(openssl_id_key)) == NULL)||
		key = setup_new_sslid(); // what if this fails? FIXME
	}
	return *key;
}
#endif

// Returns the cputype index (for use with libtorque_cpu_getdesc() of a given
// affinity ID. FIXME we ought return the cpudesc itself. That way, we could
// check the validity mask, and return NULL if it's a bad affinity ID.
unsigned libtorque_affinitymapping(const libtorque_ctx *ctx,unsigned aid){
	return ctx->affinmap[aid];
}

int associate_affinityid(libtorque_ctx *ctx,unsigned aid,unsigned idx){
	if(aid < sizeof(ctx->affinmap) / sizeof(*ctx->affinmap)){
		ctx->affinmap[aid] = idx;
		return 0;
	}
	return -1;
}

// Detect the number of processing elements (of any type) available to us; this
// isn't a function of architecture, but a function of the OS (only certain
// processors might be enabled, and we might be restricted to a subset). We
// want only those processors we can *use* (schedule code on). Hopefully, the
// OS is providing us with full use of the provided processors, simplifying our
// own scheduling (assuming we're not using measured load as a feedback).
//
// Methods to do so include:
//  - sysconf(_SC_NPROCESSORS_ONLN) (GNU extension: get_nprocs_conf())
//  - sysconf(_SC_NPROCESSORS_CONF) (GNU extension: get_nprocs())
//  - dmidecode --type 4 (Processor type)
//  - grep ^processor /proc/cpuinfo (linux only)
//  - ls /sys/devices/system/cpu/cpu? | wc -l (linux only)
//  - ls /dev/cpuctl* | wc -l (freebsd only, with cpuctl device)
//  - ls /dev/cpu/[0-9]* | wc -l (linux only, with cpuid driver)
//  - mptable (freebsd only, with SMP option)
//  - hw.ncpu, kern.smp.cpus sysctls (freebsd)
//  - read BIOS via /dev/memory (requires root)
//  - cpuset_size() (libcpuset, linux)
//  - cpuset -g (freebsd)
//  - taskset -c (linux)
//  - cpuset_getaffinity(CPU_LEVEL_CPUSET,CPU_WHICH_CPUSET) (freebsd)
//  - sched_getaffinity(0) (linux)
//  - CPUID function 0x0000_000b (x2APIC/Topology Enumeration)

// FreeBSD's cpuset.h (as of 7.2) doesn't provide CPU_COUNT, nor do older Linux
// setups (including RHEL5). This one only requires CPU_SETSIZE and CPU_ISSET.
static inline unsigned
portable_cpuset_count(const cpu_set_t *mask){
	unsigned count = 0,cpu;

	for(cpu = 0 ; cpu < CPU_SETSIZE ; ++cpu){
		if(CPU_ISSET(cpu,mask)){
			++count;
		}
	}
	return count;
}

// Returns a positive integer number of processing elements on success. A non-
// positive return value indicates failure to determine the processor count.
// A "processor" is "something on which we can schedule a running thread". On a
// successful return, mask contains the original affinity mask of the process.
static inline unsigned
detect_cpucount_internal(cpu_set_t *mask){
#ifdef LIBTORQUE_FREEBSD
	if(cpuset_getaffinity(CPU_LEVEL_CPUSET,CPU_WHICH_CPUSET,-1,
				sizeof(*mask),mask) == 0){
#elif defined(LIBTORQUE_LINUX)
	// We might be only a subthread of a larger application; use the
	// affinity mask of the thread which initializes us.
	if(pthread_getaffinity_np(pthread_self(),sizeof(*mask),mask) == 0){
#else
#error "No affinity detection method defined for this OS"
#endif
		unsigned csize;

		if((csize = portable_cpuset_count(mask)) > 0){
			return csize;
		}
	}
	return 0;
}

static unsigned
initialize_cpucount(libtorque_ctx *ctx){
	unsigned cpucount;

	CPU_ZERO(&ctx->origmask);
	if((cpucount = detect_cpucount_internal(&ctx->origmask)) <= 0){
		CPU_ZERO(&ctx->origmask);
	}
	return cpucount;
}

static inline void
copy_cpumask(cpu_set_t *dst,const cpu_set_t *src){
	memcpy(dst,src,sizeof(*dst));
}

// Ought be called before any other scheduling functions are used, and again
// after any hardware/software reconfiguration which results in a different CPU
// configuration. Returns the number of running threads which can be scheduled
// at once in the process's cpuset, or -1 on discovery failure.
unsigned detect_cpucount(libtorque_ctx *ctx,cpu_set_t *map){
	unsigned ret;

	CPU_ZERO(map);
	CPU_ZERO(&ctx->origmask);
	if((ret = initialize_cpucount(ctx)) <= 0u){
		CPU_ZERO(&ctx->origmask);
	}else{
		copy_cpumask(map,&ctx->origmask);
	}
	return ret;
}

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

// Undoes any prior pinning of this thread.
int unpin_thread(const libtorque_ctx *ctx){
#ifdef LIBTORQUE_FREEBSD
	if(cpuset_setaffinity(CPU_LEVEL_WHICH,CPU_WHICH_TID,-1,
				sizeof(ctx->origmask),&ctx->origmask)){
#else
	if(pthread_setaffinity_np(pthread_self(),
				sizeof(ctx->origmask),&ctx->origmask)){
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
	// pthread_cancel(); go ahead and do a hard test for success.
	if(pthread_cancel(tid)){
		return -1;
	}
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
	evhandler *ev = NULL;

	if((ev = create_evhandler()) == NULL){
		goto earlyerr;
	}
	if(pthread_mutex_lock(&marshal->lock)){
		destroy_evhandler(ev);
		goto earlyerr;
	}
	marshal->ctx->ev = ev;
	marshal->status = THREAD_STARTED;
	pthread_cond_broadcast(&marshal->cond);
	if(pthread_mutex_unlock(&marshal->lock)){
		goto earlyerr;
	}
	event_thread(ev);
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
int spawn_thread(libtorque_ctx *ctx,unsigned z){
	tguard tidguard = {
		.ctx = ctx,
	};
	int ret = 0;

	if(pthread_mutex_init(&tidguard.lock,NULL)){
		return -1;
	}
	if(pthread_cond_init(&tidguard.cond,NULL)){
		pthread_mutex_destroy(&tidguard.lock);
		return -1;
	}
	if(pthread_create(&ctx->tids[z],NULL,thread,&tidguard)){
		pthread_mutex_destroy(&tidguard.lock);
		pthread_cond_destroy(&tidguard.cond);
		return -1;
	}
	ret |= pthread_mutex_lock(&tidguard.lock);
	while(tidguard.status == THREAD_UNLAUNCHED){
		ret |= pthread_cond_wait(&tidguard.cond,&tidguard.lock);
	}
	if(tidguard.status != THREAD_STARTED){
		pthread_join(ctx->tids[z],NULL);
		ret = -1;
	}
	ret |= pthread_mutex_unlock(&tidguard.lock);
	ret |= pthread_cond_destroy(&tidguard.cond);
	ret |= pthread_mutex_destroy(&tidguard.lock);
	return ret;
}

int reap_threads(libtorque_ctx *ctx,unsigned tidcount){
	int ret = 0;
	unsigned z;

	for(z = 0 ; z < tidcount ; ++z){
		ret |= reap_thread(ctx->tids[z]);
	}
	return ret;
}
