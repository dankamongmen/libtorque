#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libtorque/arch.h>
#include <libtorque/x86cpuid.h>

#if defined(LIBTORQUE_LINUX)
#include <cpuset.h>
#include <sched.h>
#elif defined(LIBTORQUE_FREEBSD)
#include <sys/cpuset.h>
typedef cpusetid cpu_set_t;
#endif

// We dynamically determine whether or not advanced cpuset support (cgroups and
// the SGI libcpuset library) is available on Linux (ENOSYS or ENODEV indicate
// nay) during CPU enumeration, and only use those methods if so.
static unsigned use_cpusets;
// This is only filled in if we're using the sched_{get,set}affinity API.
static cpu_set_t orig_cpumask;

// These are valid and non-zero following a successful call of
// detect_architecture(), up until a call to free_architecture().
static unsigned cpu_typecount;
static libtorque_cput *cpudescs; // dynarray of cpu_typecount elements

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
//  - CPUID function 0x0000_000b (x2APIC/Topology Enumeration)
#ifdef LIBTORQUE_LINUX
static int
fallback_detect_cpucount(void){
	long sysonln;

	// Not the most robust method -- we prefer cpuset functions -- but it's
	// supported just about everywhere (x86info uses this method). See:
	// http://lists.openwall.net/linux-kernel/2007/04/04/183
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
detect_cpucount(cpu_set_t *mask,unsigned *cpusets){
#ifdef LIBTORQUE_FREEBSD
	int cpu,count = 0;

	if(cpuset_getaffinity(CPU_LEVEL_CPUSET,CPU_WHICH_CPUSET,-1,
				sizeof(*mask),mask) < 0){
		return -1;
	}
	for(cpu = 0 ; cpu < CPU_SETSIZE ; ++cpu){
		if(CPU_ISSET(cpu,*mask)){
			++count;
		}
	}
	return count;
#elif defined(LIBTORQUE_LINUX)
	int csize;

	if((csize = cpuset_size()) < 0){
		if(errno == ENOSYS || errno == ENODEV){
			// Cpusets aren't supported, or aren't in use
			if(sched_getaffinity(0,sizeof(*mask),mask) == 0){
				int count = CPU_COUNT(mask);

				if(count >= 1){
					return count;
				}
				return -1; // broken affinity library?
			}
			return fallback_detect_cpucount(); // pre-affinity?
		}
		return -1; // otherwise libcpuset error; die out
	}
	*cpusets = 1;
	return csize;
#else
#error "No CPU detection method defined for this OS"
	return -1;
#endif
}

// Returns the slot we just added to the end, or NULL on failure. Pointers
// will be shallow-copied; dynamically allocate them, and do not free them
// explicitly (they'll be handled by free_cpudetails()).
static inline libtorque_cput *
add_cputype(unsigned *cputc,libtorque_cput **types,
		const libtorque_cput *acpu){
	size_t s = (*cputc + 1) * sizeof(**types);
	typeof(**types) *tmp;

	if((tmp = realloc(*types,s)) == NULL){
		return NULL;
	}
	*types = tmp;
	(*types)[*cputc] = *acpu;
	return *types + (*cputc)++;
}

static void
free_cpudetails(libtorque_cput *details){
	// free(details->apicids);
	// details->apicids = NULL;
	free(details->memdescs);
	details->memdescs = NULL;
	free(details->strdescription);
	details->strdescription = NULL;
	details->stepping = details->model = details->family = 0;
	details->x86type = PROCESSOR_X86_UNKNOWN;
	details->elements = details->memories = 0;
}

// Methods to discover processor and cache details include:
//  - running CPUID (must be run on each processor, x86 only)
//  - querying cpuid(4) devices (linux only, must be root, x86 only)
//  - CPUCTL ioctl(2)s (freebsd only, with cpuctl device, x86 only)
//  - /proc/cpuinfo (linux only)
//  - /sys/devices/{system/cpu/*,/virtual/cpuid/*} (linux only)
static int
detect_cpudetails(int cpuid,libtorque_cput *details){
	if(pin_thread(cpuid)){
		return -1;
	}
	if(x86cpuid(details)){
		free_cpudetails(details);
		return -1;
	}
	return 0;
}

static int
compare_cpudetails(const libtorque_cput * restrict a,
			const libtorque_cput * restrict b){
	unsigned n;

	if(a->family != b->family || a->model != b->model ||
		a->stepping != b->stepping || a->x86type != b->x86type){
		return -1;
	}
	if(a->memories != b->memories){
		return -1;
	}
	if(strcmp(a->strdescription,b->strdescription)){
		return -1;
	}
	for(n = 0 ; n < a->memories ; ++n){
		if(memcmp(a->memdescs + n,b->memdescs + n,sizeof(*a->memdescs))){
			return -1;
		}
	}
	return 0;
}

static libtorque_cput *
match_cputype(unsigned cputc,libtorque_cput *types,
		const libtorque_cput *acpu){
	unsigned n;

	for(n = 0 ; n < cputc ; ++n){
		if(compare_cpudetails(types + n,acpu) == 0){
			return types + n;
		}
	}
	return NULL;
}

// Might leave the calling thread pinned to a particular processor; restore the
// CPU mask if necessary after a call.
static int
detect_cputypes(unsigned *cputc,libtorque_cput **types,unsigned *cpusets,
			cpu_set_t *origmask){
	int totalpe,z;

	*cputc = 0;
	*cpusets = 0;
	*types = NULL;
	CPU_ZERO(origmask);
	if((totalpe = detect_cpucount(origmask,cpusets)) <= 0){
		goto err;
	}
	for(z = 0 ; z < totalpe ; ++z){
		libtorque_cput cpudetails;
		typeof(*types) cputype;

		if(detect_cpudetails(z,&cpudetails)){
			goto err;
		}
		if( (cputype = match_cputype(*cputc,*types,&cpudetails)) ){
			++cputype->elements;
			free_cpudetails(&cpudetails);
		}else{
			cpudetails.elements = 1;
			if((cputype = add_cputype(cputc,types,&cpudetails)) == NULL){
				free_cpudetails(&cpudetails);
				goto err;
			}
		}
	}
	return 0;

err:
	CPU_ZERO(origmask);
	*cpusets = 0;
	free(*types);
	*types = NULL;
	*cputc = 0;
	return -1;
}

// Pins the current thread to the given cpuset ID, ie [0..cpuset_size()).
int pin_thread(int cpuid){
	if(use_cpusets == 0){
		cpu_set_t mask;

		CPU_ZERO(&mask);
		CPU_SET((unsigned)cpuid,&mask);
		if(sched_setaffinity(0,sizeof(mask),&mask)){
			return -1;
		}
		return 0;
	}
	return cpuset_pin(cpuid);
}

// Undoes any prior pinning of this thread.
int unpin_thread(void){
	if(use_cpusets == 0){
		if(sched_setaffinity(0,sizeof(orig_cpumask),&orig_cpumask)){
			return -1;
		}
		return 0;
	}
	return cpuset_unpin();
}

int detect_architecture(void){
	if(detect_cputypes(&cpu_typecount,&cpudescs,&use_cpusets,&orig_cpumask)){
		return -1; // FIXME unpin?
	}
	if(unpin_thread()){
		free_architecture();
		return -1;
	}
	return 0;
}

void free_architecture(void){
	CPU_ZERO(&orig_cpumask);
	use_cpusets = 0;
	while(--cpu_typecount){
		free_cpudetails(&cpudescs[cpu_typecount]);
	}
	free(cpudescs);
	cpudescs = NULL;
}

unsigned libtorque_cpu_typecount(void){
	return cpu_typecount;
}

const libtorque_cput *libtorque_cpu_getdesc(unsigned n){
	if(n >= cpu_typecount){
		return NULL;
	}
	return &cpudescs[n];
}
