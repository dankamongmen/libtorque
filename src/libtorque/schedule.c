#include <limits.h>
#include <unistd.h>
#include <libtorque/schedule.h>

// Set to 1 if we are using cpusets. Currently, this is determined at runtime
// (when built with libcpuset support, anyway). We perhaps ought just make
// libcpuset-enabled builds unconditionally use libcpuset, but that will be a
// hassle for package construction. As stands, this complicates logic FIXME.
static unsigned use_cpusets;

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
//  - taskset -c (linux)
//  - cpuset_getaffinity(CPU_LEVEL_CPUSET,CPU_WHICH_CPUSET) (freebsd)
//  - CPUID function 0x0000_000b (x2APIC/Topology Enumeration)
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

// FreeBSD's cpuset.h (as of 7.2) doesn't provide CPU_COUNT, nor do older Linux
// setups (including RHEL5). This one only requires CPU_SETSIZE and CPU_ISSET.
static inline int portable_cpuset_count(cpu_set_t *) __attribute__ ((unused));

static inline int
portable_cpuset_count(cpu_set_t *mask){
	int count = 0;
	unsigned cpu;

	for(cpu = 0 ; cpu < CPU_SETSIZE ; ++cpu){
#ifdef LIBTORQUE_LINUX
		if(CPU_ISSET(cpu,mask)){
#else
		if(CPU_ISSET(cpu,*mask)){
#endif
			++count;
		}
	}
	return count;
}

// Returns a positive integer number of processing elements on success. A non-
// positive return value indicates failure to determine the processor count.
// A "processor" is "something on which we can schedule a running thread". On a
// successful return, mask contains the original affinity mask of the process.
int detect_cpucount(cpu_set_t *mask){
#ifdef LIBTORQUE_FREEBSD
	int count;

	CPU_ZERO(mask);
	if(cpuset_getaffinity(CPU_LEVEL_CPUSET,CPU_WHICH_CPUSET,-1,
				sizeof(*mask),mask) < 0){
		return -1;
	}
	if((count = portable_cpuset_count(mask)) <= 0){ // broken cpusets...?
		return fallback_detect_cpucount();
	}
	return count;
#elif defined(LIBTORQUE_LINUX)
	int csize;

#ifdef LIBTORQUE_WITH_CPUSET
	if((csize = cpuset_size()) < 0){
		if(errno == ENOSYS || errno == ENODEV){
			// Cpusets aren't supported, or aren't in use
#endif
			if(sched_getaffinity(0,sizeof(*mask),mask) == 0){
#ifdef CPU_COUNT // FIXME what if CPU_COUNT is a function, not a macro?
				int count = CPU_COUNT(mask);
#else
				int count = portable_cpuset_count(mask);
#endif

				if(count >= 1){
					return count;
				}
				return -1; // broken affinity library?
			}
			return fallback_detect_cpucount(); // pre-affinity?
#ifdef LIBTORQUE_WITH_CPUSET
		}
		return -1; // otherwise libcpuset error; die out
	}
#endif
	use_cpusets = 1;
	return csize;
#else
#error "No CPU detection method defined for this OS"
	return -1;
#endif
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
#ifdef LIBTORQUE_WITH_CPUSET
	return cpuset_pin(cpuid);
#else
	return -1;
#endif
}

// Undoes any prior pinning of this thread.
int unpin_thread(cpu_set_t *origmask){
	if(use_cpusets == 0){
		if(sched_setaffinity(0,sizeof(*origmask),origmask)){
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

