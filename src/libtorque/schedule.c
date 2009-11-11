#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <libtorque/internal.h>
#include <libtorque/events/thread.h>

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
portable_cpuset_count(libtorque_ctx *ctx,const cpu_set_t *mask){
	unsigned count = 0,cpu;

	for(cpu = 0 ; cpu < CPU_SETSIZE ; ++cpu){
		if(CPU_ISSET(cpu,mask)){
			ctx->tiddata[count++].affinity_id = cpu;
		}
	}
	return count;
}

// Returns a positive integer number of processing elements on success. A non-
// positive return value indicates failure to determine the processor count.
// A "processor" is "something on which we can schedule a running thread". On a
// successful return, mask contains the original affinity mask of the process.
static inline unsigned
detect_cpucount_internal(libtorque_ctx *ctx,cpu_set_t *mask){
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

		if((csize = portable_cpuset_count(ctx,mask)) > 0){
			return csize;
		}
	}
	return 0;
}

static unsigned
initialize_cpucount(libtorque_ctx *ctx){
	unsigned cpucount;

	CPU_ZERO(&ctx->origmask);
	if((cpucount = detect_cpucount_internal(ctx,&ctx->origmask)) <= 0){
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
reap_thread(pthread_t tid,tdata *tstate){
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
	ret |= pthread_mutex_destroy(&tstate->lock);
	ret |= pthread_cond_destroy(&tstate->cond);
	tstate->status = THREAD_UNLAUNCHED;
	return 0;
}

static void *
thread(void *void_marshal){
	tdata *marshal = void_marshal;

	if(pin_thread(marshal->affinity_id)){
		goto earlyerr;
	}
	if(pthread_mutex_lock(&marshal->lock)){
		goto earlyerr;
	}
	marshal->status = THREAD_STARTED;
	if(pthread_mutex_unlock(&marshal->lock)){
		goto earlyerr;
	}
	if(pthread_cond_broadcast(&marshal->cond)){
		goto earlyerr;
	}
	event_thread();
	return NULL;

earlyerr:
	pthread_mutex_lock(&marshal->lock); // continue regardless
	marshal->status = THREAD_PREFAIL;
	pthread_mutex_unlock(&marshal->lock);
	pthread_cond_broadcast(&marshal->cond);
	return NULL;
}

// FIXME launch these as we're skipping around CPU's in initial detection!
int spawn_threads(libtorque_ctx *ctx){
	unsigned z;

	ctx->tidcount = 0;
	for(z = 0 ; z < ctx->cpucount ; ++z){
		if(pthread_mutex_init(&ctx->tiddata[z].lock,NULL)){
			goto err;
		}
		if(pthread_cond_init(&ctx->tiddata[z].cond,NULL)){
			pthread_mutex_destroy(&ctx->tiddata[z].lock);
			goto err;
		}
		if(pthread_create(&ctx->tids[z],NULL,thread,&ctx->tiddata[z])){
			pthread_mutex_destroy(&ctx->tiddata[z].lock);
			pthread_cond_destroy(&ctx->tiddata[z].cond);
			goto err;
		}
		pthread_mutex_lock(&ctx->tiddata[z].lock);
		while(ctx->tiddata[z].status == THREAD_UNLAUNCHED){
			pthread_cond_wait(&ctx->tiddata[z].cond,&ctx->tiddata[z].lock);
		}
		if(ctx->tiddata[z].status != THREAD_STARTED){
			pthread_mutex_unlock(&ctx->tiddata[z].lock);
			pthread_join(ctx->tids[z],NULL);
			pthread_mutex_destroy(&ctx->tiddata[z].lock);
			pthread_cond_destroy(&ctx->tiddata[z].cond);
			goto err;
		}
		pthread_mutex_unlock(&ctx->tiddata[z].lock);
	}
	ctx->tidcount = z;
	return 0;

err:
	while(z--){
		reap_thread(ctx->tids[z],&ctx->tiddata[z]);
	}
	return -1;
}

int reap_threads(libtorque_ctx *ctx){
	int ret = 0;
	unsigned z;

	for(z = 0 ; z < ctx->tidcount ; ++z){
		ret |= reap_thread(ctx->tids[z],&ctx->tiddata[z]);
	}
	ctx->tidcount = 0;
	return ret;
}
