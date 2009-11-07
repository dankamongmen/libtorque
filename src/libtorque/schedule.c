#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <libtorque/schedule.h>
#include <libtorque/events/thread.h>

// Set to 1 if we are using cpusets. Currently, this is determined at runtime
// (when built with libcpuset support, anyway). We perhaps ought just make
// libcpuset-enabled builds unconditionally use libcpuset, but that will be a
// hassle for package construction. As stands, this complicates logic FIXME.
static unsigned use_cpusets;

typedef struct tdata {
	unsigned affinity_id;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	enum {
		THREAD_UNLAUNCHED = 0,
		THREAD_PREFAIL,
		THREAD_STARTED,
	} status;
} tdata;

#ifdef CPU_ALLOC_SIZE
#define CPUSET_BYTES CPU_ALLOC_SIZE
#else
#define CPUSET_BYTES sizeof(cpu_set_t)
#endif

static pthread_once_t cpucount_once = PTHREAD_ONCE_INIT;
// Set whenever detect_cpucount() is called, which generally only happens once.
// Who knows what hotpluggable CPUs the future brings, and what interfaces
// we'll need? For now, ensure this via a pthread_once_t.
static unsigned cpucount;
static cpu_set_t origmask;
// This is fairly wasteful, and broken for libcpuset (which requires us to use
// cpuset_size()). Allocate them upon detecting cpu count FIXME.
static tdata tiddata[CPU_SETSIZE];
static pthread_t tids[CPU_SETSIZE];
static unsigned affinityid_map[CPU_SETSIZE];	// maps into the cpu desc table

unsigned libtorque_affinitymapping(unsigned aid){
	return affinityid_map[aid];
}

int associate_affinityid(unsigned aid,unsigned idx){
	if(aid < sizeof(affinityid_map) / sizeof(*affinityid_map)){
		affinityid_map[aid] = idx;
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
portable_cpuset_count(cpu_set_t *mask){
#ifdef CPU_COUNT // what if it's a function, not a macro? it'll go unused...
	int ret;

	if((ret = CPU_COUNT(mask)) <= 0){
		return 0;
	}
	return (unsigned)ret;
#else
	unsigned count = 0,cpu;

	for(cpu = 0 ; cpu < CPU_SETSIZE ; ++cpu){
		if(CPU_ISSET(cpu,mask)){
			tiddata[count++].affinity_id = cpu;
		}
	}
	return count;
#endif
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
		unsigned count;

		if((count = portable_cpuset_count(mask)) > 0){
			return count;
		}
	}
#elif defined(LIBTORQUE_LINUX)
	unsigned csize;

	// We might be only a subthread of a larger application; use the
	// affinity mask of the thread which initializes us.
	if(pthread_getaffinity_np(pthread_self(),sizeof(*mask),mask) == 0){
		if((csize = portable_cpuset_count(mask)) > 0){
#ifdef LIBTORQUE_WITH_CPUSET
			int cs;

			if((cs = cpuset_size()) <= 0){
				// Fall through if cpusets aren't supported, or aren't in use
				if(csize == 0 || (errno != ENOSYS && errno != ENODEV)){
					return 0;
				}
			}else if(cs != csize){ // ensure equality of counts
				return 0;
			}
			use_cpusets = 1;
#endif
			return csize;
		}
	}
#else
#error "No CPU detection method defined for this OS"
#endif
	return 0;
}

static void
initialize_cpucount(void){
	CPU_ZERO(&origmask);
	if((cpucount = detect_cpucount_internal(&origmask)) <= 0){
		CPU_ZERO(&origmask);
	}
}

static inline void
copy_cpumask(cpu_set_t *dst,const cpu_set_t *src){
	memcpy(dst,src,CPUSET_BYTES);
}

// Ought be called before any other scheduling functions are used, and again
// after any hardware/software reconfiguration which results in a different CPU
// configuration. Returns the number of running threads which can be scheduled
// at once in the process's cpuset, or -1 on discovery failure.
unsigned detect_cpucount(cpu_set_t *map){
	CPU_ZERO(map);
	if(pthread_once(&cpucount_once,initialize_cpucount) == 0){
		copy_cpumask(map,&origmask);
		return cpucount;
	}
	return 0;
}

// Pins the current thread to the given cpuset ID, ie [0..cpuset_size()).
int pin_thread(unsigned aid){
	if(use_cpusets == 0){
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
		if(cpuset_setaffinity(CPU_LEVEL_WHICH,CPU_WHICH_TID,-1,
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
