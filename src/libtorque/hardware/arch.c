#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <libtorque/internal.h>
#include <libtorque/hardware/cuda.h>
#include <libtorque/hardware/arch.h>
#include <libtorque/hardware/memory.h>
#include <libtorque/hardware/x86cpuid.h>
#include <libtorque/hardware/topology.h>

// Returns the slot we just added to the end, or NULL on failure. Pointers
// will be shallow-copied; dynamically allocate them, and do not free them
// explicitly (they'll be handled by free_cpudetails()).
static inline torque_cput *
add_cputype(unsigned *cputc,torque_cput **types,
		const torque_cput *acpu){
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
free_cpudetails(torque_cput *details){
	free(details->tlbdescs);
	details->tlbdescs = NULL;
	free(details->memdescs);
	details->memdescs = NULL;
	free(details->strdescription);
	details->strdescription = NULL;
	memset(&details->spec,0,sizeof(details->spec));
	details->tlbs = details->elements = details->memories = 0;
	details->coresperpackage = details->threadspercore = 0;
}

// Methods to discover processor and cache details include:
//  - running CPUID (must be run on each processor, x86 only)
//  - querying cpuid(4) devices (linux only, must be root, x86 only)
//  - CPUCTL ioctl(2)s (freebsd only, with cpuctl device, x86 only)
//  - /proc/cpuinfo (linux only)
//  - /sys/devices/{system/cpu/*,/virtual/cpuid/*} (linux only)
static torque_err
detect_cpudetails(unsigned id,torque_cput *details,
			unsigned *thread,unsigned *core,unsigned *pkg){
	if(pin_thread(id)){
		return TORQUE_ERR_AFFINITY;
	}
	if(x86cpuid(details,thread,core,pkg)){
		free_cpudetails(details);
		return TORQUE_ERR_CPUDETECT;
	}
	return 0;
}

static int
compare_cpudetails(const torque_cput * restrict a,
			const torque_cput * restrict b){
	if(memcmp(&a->spec,&b->spec,sizeof(a->spec))){
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
	if(memcmp(a->tlbdescs,b->tlbdescs,a->tlbs * sizeof(*a->tlbdescs))){
		return -1;
	}
	if(memcmp(a->memdescs,b->memdescs,a->memories * sizeof(*a->memdescs))){
		return -1;
	}
	return 0;
}

static torque_cput *
match_cputype(unsigned cputc,torque_cput *types,
		const torque_cput *acpu){
	unsigned n;

	for(n = 0 ; n < cputc ; ++n){
		if(compare_cpudetails(types + n,acpu) == 0){
			return types + n;
		}
	}
	return NULL;
}

// Revert to the original cpuset mask
static int
unpin_thread(const cpu_set_t *cset){
#ifdef TORQUE_FREEBSD
	if(cpuset_setaffinity(CPU_LEVEL_WHICH,CPU_WHICH_TID,-1,sizeof(*cset),cset)){
#else
	if(pthread_setaffinity_np(pthread_self(),sizeof(*cset),cset)){
#endif
		return -1;
	}
	return 0;
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

// Returns a positive integer number of processing elements on success. A non-
// positive return value indicates failure to determine the processor count.
// A "processor" is "something on which we can schedule a running thread". On a
// successful return, mask contains the original affinity mask of the process.
static inline unsigned
detect_cpucount(cpu_set_t *mask){
#ifdef TORQUE_FREEBSD
	if(cpuset_getaffinity(CPU_LEVEL_CPUSET,CPU_WHICH_CPUSET,-1,
				sizeof(*mask),mask) == 0){
#elif defined(TORQUE_LINUX)
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

// Might leave the calling thread pinned to a particular processor; restore the
// CPU mask if necessary after a call.
static torque_err
detect_cputypes(torque_ctx *ctx,unsigned *cputc,torque_cput **types){
	struct top_map *topmap = NULL;
	unsigned z,aid,cpucount;
	torque_err ret;
	cpu_set_t mask;

	*cputc = 0;
	*types = NULL;
	// we're basically doing this in the main loop. purge! FIXME
	if((cpucount = detect_cpucount(&mask)) <= 0){
		ret = TORQUE_ERR_AFFINITY;
		goto err;
	}
	// Might be quite large; we don't want it allocated on the stack
	if((topmap = malloc(cpucount * sizeof(*topmap))) == NULL){
		ret = TORQUE_ERR_RESOURCE;
		goto err;
	}
	memset(topmap,0,cpucount * sizeof(*topmap));
	// FIXME parallelize this (see bug 24) -- move the code before
	// spawn_thread() into it, and make it thread safe.
	for(z = 0, aid = 0 ; z < cpucount ; ++z){
		torque_cput cpudetails;
		unsigned thread,core,pkg;
		typeof(*types) cputype;

		while(aid < CPU_SETSIZE && !CPU_ISSET(aid,&mask)){
			++aid;
		}
		if(aid == CPU_SETSIZE){
			ret = TORQUE_ERR_AFFINITY;
			goto err;
		}
		if( (ret = detect_cpudetails(aid,&cpudetails,&thread,&core,&pkg)) ){
			goto err;
		}
		if( (cputype = match_cputype(*cputc,*types,&cpudetails)) ){
			++cputype->elements;
			free_cpudetails(&cpudetails);
		}else{
			cpudetails.elements = 1;
			if((cputype = add_cputype(cputc,types,&cpudetails)) == NULL){
				free_cpudetails(&cpudetails);
				ret = TORQUE_ERR_RESOURCE;
				goto err;
			}
		}
		if( (ret = topologize(ctx,topmap,aid,thread,core,pkg,(unsigned)(cputype - *types))) ){
			goto err;
		}
		if(spawn_thread(ctx)){
			ret = TORQUE_ERR_RESOURCE;
			goto err;
		}
		++aid;
	}
	if(unpin_thread(&mask)){
		ret = TORQUE_ERR_AFFINITY;
		goto err;
	}
	free(topmap);
	return 0;

err:
	unpin_thread(&mask);
	reap_threads(ctx);
	while((*cputc)--){
		free_cpudetails((*types) + ctx->cpu_typecount);
	}
	*cputc = 0;
	free(*types);
	*types = NULL;
	free(topmap);
	return ret;
}

static torque_err
detect_cuda(void){
	int devs;

	if((devs = detect_cudadevcount()) < 0){
		return TORQUE_ERR_GPUDETECT;
	}
	return 0;
}

torque_err detect_architecture(torque_ctx *ctx){
	torque_err ret;

	if( (ret = detect_cputypes(ctx,&ctx->cpu_typecount,&ctx->cpudescs)) ){
		goto err;
	}
	if( (ret = detect_cuda()) ){
		goto err;
	}
	if(detect_memories(ctx)){
		ret = TORQUE_ERR_MEMDETECT;
		goto err;
	}
	return 0;

err:
	free_architecture(ctx);
	return ret;
}

void free_architecture(torque_ctx *ctx){
	reset_topology(ctx);
	while(ctx->cpu_typecount--){
		free_cpudetails(&ctx->cpudescs[ctx->cpu_typecount]);
	}
	ctx->cpu_typecount = 0;
	free(ctx->cpudescs);
	ctx->cpudescs = NULL;
	free_memories(ctx);
}

unsigned torque_cpu_typecount(const torque_ctx *ctx){
	return ctx->cpu_typecount;
}

// Takes a description ID
const torque_cput *torque_cpu_getdesc(const torque_ctx *ctx,unsigned n){
	if(n >= ctx->cpu_typecount){
		return NULL;
	}
	return &ctx->cpudescs[n];
}
