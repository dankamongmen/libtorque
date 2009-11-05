#include <unistd.h>
#include <pthread.h>
#include <libtorque/schedule.h>
#include <libtorque/events/thread.h>

// Set to 1 if we are using cpusets. Currently, this is determined at runtime
// (when built with libcpuset support, anyway). We perhaps ought just make
// libcpuset-enabled builds unconditionally use libcpuset, but that will be a
// hassle for package construction. As stands, this complicates logic FIXME.
static unsigned use_cpusets;

typedef struct tdata {
	int affinity_id;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	enum {
		THREAD_UNLAUNCHED = 0,
		THREAD_PREFAIL,
		THREAD_STARTED,
	} status;
} tdata;

// Set whenever detect_cpucount() is called, which generally only happen once
// (we don't enforce this; who knows what hotpluggable CPUs the future brings,
// and what interfaces we'll need?)
static unsigned cpucount;
static cpu_set_t origmask;
// This is fairly wasteful, and broken for libcpuset (which requires us to use
// cpuset_size()). Allocate them upon detecting cpu count FIXME.
static tdata tiddata[CPU_SETSIZE];
static pthread_t tids[CPU_SETSIZE];

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
static inline int
portable_cpuset_count(cpu_set_t *mask){
	int count = 0,cpu;

	for(cpu = 0 ; cpu < CPU_SETSIZE ; ++cpu){
		if(CPU_ISSET((unsigned)cpu,mask)){
			tiddata[count++].affinity_id = cpu;
		}else{
			tiddata[count].affinity_id = -1;
		}
	}
	return count;
}

// Returns a positive integer number of processing elements on success. A non-
// positive return value indicates failure to determine the processor count.
// A "processor" is "something on which we can schedule a running thread". On a
// successful return, mask contains the original affinity mask of the process.
static inline int
detect_cpucount_internal(cpu_set_t *mask){
#ifdef LIBTORQUE_FREEBSD
	if(cpuset_getaffinity(CPU_LEVEL_CPUSET,CPU_WHICH_CPUSET,-1,
				sizeof(*mask),mask) == 0){
		int count;

		if((count = portable_cpuset_count(mask)) > 0){
			return count;
		}
	}
#elif defined(LIBTORQUE_LINUX)
	int csize;

#ifdef LIBTORQUE_WITH_CPUSET
	if((csize = cpuset_size()) <= 0){
		// Fall through if cpusets aren't supported, or aren't in use
		if(csize == 0 || (errno != ENOSYS && errno != ENODEV)){
			return -1;
		}
	}else{
		use_cpusets = 1;
		return csize;
	}
#endif
	if(pthread_getaffinity_np(pthread_self(),sizeof(*mask),mask) == 0){
		if((csize = portable_cpuset_count(mask)) > 0){
			return csize;
		}
	}
#else
#error "No CPU detection method defined for this OS"
#endif
	return -1;
}

// Ought be called before any other scheduling functions are used, and again
// after any hardware/software reconfiguration which results in a different CPU
// configuration. Returns the number of running threads which can be scheduled
// at once in the process's cpuset, or -1 on discovery failure.
int detect_cpucount(void){
	int ret;

	CPU_ZERO(&origmask);
	if((ret = detect_cpucount_internal(&origmask)) > 0){
		cpucount = (unsigned)ret;
		return ret;
	}
	CPU_ZERO(&origmask);
	return -1;
}

// Pins the current thread to the given cpuset ID, ie [0..cpuset_size()).
int pin_thread(int cpuid){
	if(use_cpusets == 0){
		cpu_set_t mask;

		CPU_ZERO(&mask);
		CPU_SET((unsigned)cpuid,&mask);
#ifdef LIBTORQUE_FREEBSD
		if(cpuset_setaffinity(CPU_LEVEL_CPUSET,CPU_WHICH_CPUSET,-1,
					sizeof(mask),&mask)){
#else
		if(pthread_setaffinity_np(pthread_self(),sizeof(mask),&mask)){
#endif
			return -1;
		}
		return 0;
	}
#ifdef LIBTORQUE_WITH_CPUSET
	return cpuset_pin(cpuid);
#else
	return -1;
#endif
}

// Undoes any prior pinning of this thread.
int unpin_thread(void){
	if(use_cpusets == 0){
#ifdef LIBTORQUE_FREEBSD
		if(cpuset_setaffinity(CPU_LEVEL_CPUSET,CPU_WHICH_CPUSET,-1,
					sizeof(origmask),&origmask)){
#else
		if(pthread_setaffinity_np(pthread_self(),sizeof(origmask),
						&origmask)){
#endif
			return -1;
		}
		return 0;
	}
#ifdef LIBTORQUE_WITH_CPUSET
	return cpuset_unpin();
#else
	return -1;
#endif
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

int spawn_threads(void){
	unsigned z;

	for(z = 0 ; z < cpucount ; ++z){
		if(pthread_mutex_init(&tiddata[z].lock,NULL)){
			goto err;
		}
		if(pthread_cond_init(&tiddata[z].cond,NULL)){
			pthread_mutex_destroy(&tiddata[z].lock);
			goto err;
		}
		if(pthread_create(&tids[z],NULL,thread,&tiddata[z])){
			pthread_mutex_destroy(&tiddata[z].lock);
			pthread_cond_destroy(&tiddata[z].cond);
			goto err;
		}
		pthread_mutex_lock(&tiddata[z].lock);
		while(tiddata[z].status == THREAD_UNLAUNCHED){
			pthread_cond_wait(&tiddata[z].cond,&tiddata[z].lock);
		}
		if(tiddata[z].status != THREAD_STARTED){
			pthread_join(tids[z],NULL);
			pthread_mutex_destroy(&tiddata[z].lock);
			pthread_cond_destroy(&tiddata[z].cond);
			goto err;
		}
		pthread_mutex_unlock(&tiddata[z].lock);
	}
	return 0;

err:
	while(z--){
		reap_thread(tids[z],&tiddata[z]);
	}
	return -1;
}

int reap_threads(void){
	int ret = 0;
	unsigned z;

	for(z = 0 ; z < cpucount ; ++z){
		ret |= reap_thread(tids[z],&tiddata[z]);
	}
	return ret;
}
