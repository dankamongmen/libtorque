#include <string.h>
#include <stdlib.h>
#include <libtorque/arch.h>

static unsigned cpu_typecount;
static libtorque_cputype *cpudescs;

// Detecting the number of processing units available doesn't rely on
// architecture (though we could, perhaps, check that for inconsistencies);
// what's important is what the configured operating system is providing us.
// We detect only those processing units we can *use*.
//
// Methods to do so include:
//  - sysconf(_SC_NPROCESSORS_ONLN) (GNU extension: get_nprocs_conf())
//  - sysconf(_SC_NPROCESSORS_CONF) (GNU extension: get_nprocs())
//  - dmidecode --type 4 (Processor)
//  - grep ^processor /proc/cpuinfo (linux only)
//  - ls /sys/devices/system/cpu/cpu? | wc -l (linux only)
//  - ls /dev/cpuctl* | wc -l (freebsd only, with cpuctl device)
//  - ls /dev/cpu/[0-9]* | wc -l (linux only, with cpuid driver)
//  - mptable (freebsd only, with SMP option)
//  - hw.ncpu, kern.smp.cpus sysctls (freebsd)
//  - cpuset_size() (libcpuset, linux)
//  - cpuset_getid(CPU_LEVEL_ROOT,CPU_WHICH_CPUSET) (freebsd)

static int
detect_cputypes(unsigned *cputc,libtorque_cputype **types){
	// FIXME we'll want to support heterogenous setups sooner rather than
	// later. what we ought do is detect the # of processing units, and
	// then run discovery on each one. if they are equal, they're the same
	// type. if not, they're not. later, if discovery APIs change, we can
	// adapt to that easily.
	*cputc = 1;
	if((*types = malloc(sizeof(**types) * *cputc)) == NULL){
		return -1;
	}
	memset(*types,0,sizeof(**types) * *cputc);
	return 0;
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

unsigned libtorque_cpu_typecount(void){
	return cpu_typecount;
}

const libtorque_cputype *libtorque_cpu_getdesc(unsigned n){
	if(n >= cpu_typecount){
		return NULL;
	}
	return &cpudescs[n];
}
