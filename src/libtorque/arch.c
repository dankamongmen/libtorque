#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libtorque/arch.h>

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
//  - cpuset_getid(CPU_LEVEL_CPUSET,CPU_WHICH_CPUSET) (freebsd)
static inline int
detect_cpucount(void){
	long sysonln;

	// FIXME not the most robust method -- we'd rather use cpuset functions
	// directly. freebsd's cpuset doesn't give us such a function; we must
	// test each possible processor for membership in the set(!); see
	// /usr.bin/cpuset/cpuset.c in a 7.2 checkout.
	if((sysonln = sysconf(_SC_NPROCESSORS_ONLN)) <= 0){
		return -1;
	}
	if(sysonln > INT_MAX){
		return -1;
	}
	return (int)sysonln;
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
	// FIXME schedule ourselves on this PE and detect details, purge this
	memset(details,0,sizeof(*details));
	cpuid = 0;
	return 0;
}

static int
compare_cpudetails(const libtorque_cputype * restrict a,
			const libtorque_cputype * restrict b){
	unsigned n;

	if(a->caches != b->caches){
		return -1;
	}
	for(n = 0 ; n < a->caches ; ++n){
		if(memcmp(a->cachedescs + n,b->cachedescs + n,
				sizeof(*a->cachedescs))){
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
	return 0;

err:
	free(*types);
	*types = NULL;
	*cputc = 0;
	return -1;
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

// FIXME ought provide for cleaning up the detection state!

unsigned libtorque_cpu_typecount(void){
	return cpu_typecount;
}

const libtorque_cputype *libtorque_cpu_getdesc(unsigned n){
	if(n >= cpu_typecount){
		return NULL;
	}
	return &cpudescs[n];
}
