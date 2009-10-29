#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libtorque/arch.h>

#if defined(LIBTORQUE_LINUX)
#include <cpuset.h>
#elif defined(LIBTORQUE_FREEBSD)
#include <sys/cpuset.h>
#endif

static unsigned cpu_typecount;
static libtorque_cputype *cpudescs;

// LibNUMA looks like the only real candidate for NUMA discovery (linux only)

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
//  - cpuset_size() (libcpuset, linux)
//  - cpuset -g (freebsd)
//  - cpuset_getaffinity(CPU_LEVEL_CPUSET,CPU_WHICH_CPUSET) (freebsd)
#ifdef LIBTORQUE_LINUX
static int
fallback_detect_cpucount(void){
	long sysonln;

	// not the most robust method -- we prefer cpuset functions
	if((sysonln = sysconf(_SC_NPROCESSORS_ONLN)) <= 0){
		return -1;
	}
	if(sysonln > INT_MAX){
		return -1;
	}
	return (int)sysonln;
}
#endif

static inline int
detect_cpucount(void){
#ifdef LIBTORQUE_FREEBSD
#error "No CPU detection method defined for this OS"
	int cpu,count = 0;
	cpuset_t mask;

	if(cpuset_getaffinity(CPU_LEVEL_CPUSET,CPU_WHICH_CPUSET,-1,
				sizeof(mask),&mask) < 0){
		return -1;
	}
	for(cpu = 0 ; cpu < CPU_SETSIZE ; ++cpu){
		if(CPU_ISSET(cpu,mask)){
			++count;
		}
	}
	return count;
#elif defined(LIBTORQUE_LINUX)
	int csize;

	if((csize = cpuset_size()) < 0){
		if(errno == ENOSYS || errno == ENODEV){
			// Cpusets aren't supported, or aren't in use
			return fallback_detect_cpucount();
		}
		return -1; // otherwise libcpuset error; die out
	}
	return csize;
#else
#error "No CPU detection method defined for this OS"
	return -1;
#endif
}

// Returns the slot we just added to the end, or NULL on failure.
static inline libtorque_cputype *
add_cputype(unsigned *cputc,libtorque_cputype **types,
		const libtorque_cputype *acpu){
	size_t s = (*cputc + 1) * sizeof(**types);
	typeof(**types) *tmp;

	if((tmp = realloc(*types,s)) == NULL){
		return NULL;
	}
	*types = tmp;
	(*types)[*cputc] = *acpu;
	return *types + (*cputc)++;
}

// Methods to discover processor and cache details include:
//  - running CPUID (must be run on each processor, x86 only)
//  - querying cpuid(4) devices (linux only, must be root, x86 only)
//  - CPUCTL ioctl(2)s (freebsd only, with cpuctl device, x86 only)
//  - /proc/cpuinfo (linux only)
//  - /sys/devices/{system/cpu/*,/virtual/cpuid/*} (linux only)
static int
detect_cpudetails(int cpuid,libtorque_cputype *details){
	if(cpuset_pin(cpuid)){
		return -1;
	}
	// FIXME detect details for pinned processor, and purge this
	memset(details,0,sizeof(*details));
	cpuid = 0;
	return 0;
}

static int
compare_cpudetails(const libtorque_cputype * restrict a,
			const libtorque_cputype * restrict b){
	unsigned n;

	if(a->memories != b->memories){
		return -1;
	}
	for(n = 0 ; n < a->memories ; ++n){
		if(memcmp(a->memdescs + n,b->memdescs + n,sizeof(*a->memdescs))){
			return -1;
		}
	}
	return 0;
}

static libtorque_cputype *
match_cputype(unsigned cputc,libtorque_cputype *types,
		const libtorque_cputype *acpu){
	unsigned n;

	for(n = 0 ; n < cputc ; ++n){
		if(compare_cpudetails(types + n,acpu) == 0){
			return types + n;
		}
	}
	return NULL;
}

static int
detect_cputypes(unsigned *cputc,libtorque_cputype **types){
	int totalpe,z;

	*cputc = 0;
	*types = NULL;
	if((totalpe = detect_cpucount()) <= 0){
		goto err;
	}
	for(z = 0 ; z < totalpe ; ++z){
		libtorque_cputype cpudetails;
		typeof(*types) cputype;

		if(detect_cpudetails(z,&cpudetails)){
			goto err;
		}
		if( (cputype = match_cputype(*cputc,*types,&cpudetails)) ){
			++cputype->elements;
		}else{
			cpudetails.elements = 1;
			if((cputype = add_cputype(cputc,types,&cpudetails)) == NULL){
				goto err;
			}
		}
	}
	// FIXME at this point, we're running pinned to the last processor on
	// which we ran detect_cpudetails(). reset our affinity mask!
	return 0;

err:
	free(*types);
	*types = NULL;
	*cputc = 0;
	return -1;
}

// Pins the current thread to the given cpuset ID, ie [0..cpuset_size()).
int pin_thread(int cpusetid){
	return cpuset_pin(cpusetid);
}

int detect_architecture(void){
	libtorque_cputype *cpud;
	unsigned cputc;

	if(detect_cputypes(&cputc,&cpud)){
		return -1;
	}
	cpu_typecount = cputc;
	cpudescs = cpud;
	return 0;
}

void free_architecture(void){
	free(cpudescs);
	cpudescs = NULL;
	cpu_typecount = 0;
}

unsigned libtorque_cpu_typecount(void){
	return cpu_typecount;
}

const libtorque_cputype *libtorque_cpu_getdesc(unsigned n){
	if(n >= cpu_typecount){
		return NULL;
	}
	return &cpudescs[n];
}
