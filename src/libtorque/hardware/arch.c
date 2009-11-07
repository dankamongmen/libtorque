#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <libtorque/schedule.h>
#include <libtorque/hardware/arch.h>
#include <libtorque/hardware/memory.h>
#include <libtorque/hardware/x86cpuid.h>
#include <libtorque/hardware/topology.h>

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
	free(details->tlbdescs);
	details->tlbdescs = NULL;
	free(details->memdescs);
	details->memdescs = NULL;
	free(details->strdescription);
	details->strdescription = NULL;
	details->stepping = details->model = details->family = 0;
	details->tlbs = details->elements = details->memories = 0;
	details->coresperpackage = details->threadspercore = 0;
	details->x86type = PROCESSOR_X86_UNKNOWN;
}

// Methods to discover processor and cache details include:
//  - running CPUID (must be run on each processor, x86 only)
//  - querying cpuid(4) devices (linux only, must be root, x86 only)
//  - CPUCTL ioctl(2)s (freebsd only, with cpuctl device, x86 only)
//  - /proc/cpuinfo (linux only)
//  - /sys/devices/{system/cpu/*,/virtual/cpuid/*} (linux only)
static int
detect_cpudetails(unsigned id,libtorque_cput *details,unsigned *thread,
			unsigned *core,unsigned *pkg){
	if(pin_thread(id)){
		return -1;
	}
	if(x86cpuid(details,thread,core,pkg)){
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
	if(a->memories != b->memories || a->tlbs != b->tlbs){
		return -1;
	}
	if(a->threadspercore != b->threadspercore || a->coresperpackage != b->coresperpackage){
		return -1;
	}
	if(strcmp(a->strdescription,b->strdescription)){
		return -1;
	}
	for(n = 0 ; n < a->tlbs ; ++n){
		if(memcmp(a->tlbdescs + n,b->tlbdescs + n,sizeof(*a->tlbdescs))){
			return -1;
		}
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
detect_cputypes(unsigned *cputc,libtorque_cput **types){
	unsigned totalpe,z,cpu;
	cpu_set_t mask;

	*cputc = 0;
	*types = NULL;
	if((totalpe = detect_cpucount(&mask)) <= 0){
		goto err;
	}
	for(z = 0, cpu = 0 ; z < totalpe ; ++z){
		libtorque_cput cpudetails;
		unsigned thread,core,pkg;
		typeof(*types) cputype;

		while(cpu < CPU_SETSIZE && !CPU_ISSET(cpu,&mask)){
			++cpu;
		}
		if(cpu == CPU_SETSIZE){
			goto err;
		}
		if(detect_cpudetails(cpu,&cpudetails,&thread,&core,&pkg)){
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
		if(associate_affinityid(cpu,(unsigned)(cputype - *types)) < 0){
			goto err;
		}
		if(topologize(cpu,thread,core,pkg)){
			goto err;
		}
		++cpu;
	}
	if(unpin_thread()){
		goto err;
	}
	return 0;

err:
	unpin_thread();
	while((*cputc)--){
		free_cpudetails((*types) + cpu_typecount);
	}
	*cputc = 0;
	free(*types);
	*types = NULL;
	return -1;
}

int detect_architecture(void){
	if(detect_memories()){
		goto err;
	}
	if(detect_cputypes(&cpu_typecount,&cpudescs)){
		goto err;
	}
	return 0;

err:
	free_architecture();
	return -1;
}

void free_architecture(void){
	reset_topology();
	CPU_ZERO(&orig_cpumask);
	use_cpusets = 0;
	while(cpu_typecount--){
		free_cpudetails(&cpudescs[cpu_typecount]);
	}
	cpu_typecount = 0;
	free(cpudescs);
	cpudescs = NULL;
	free_memories();
}

unsigned libtorque_cpu_typecount(void){
	return cpu_typecount;
}

// Takes a description ID
const libtorque_cput *libtorque_cpu_getdesc(unsigned n){
	if(n >= cpu_typecount){
		return NULL;
	}
	return &cpudescs[n];
}
