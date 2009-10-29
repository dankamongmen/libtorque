#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <libtorque/x86cpuid.h>

int cpuid_available(void){
	const unsigned long flag = 0x200000;
	unsigned long f1, f2;

	__asm__ volatile(
		"pushf\n\t"
		"pushf\n\t"
		"pop %0\n\t"
		"mov %0,%1\n\t"
		"xor %2,%0\n\t"
		"push %0\n\t"
		"popf\n\t"
		"pushf\n\t"
		"pop %0\n\t"
		"popf\n\t"
		: "=&r" (f1), "=&r" (f2)
		: "ir" (flag)
	);
	return ((f1 ^ f2) & flag) != 0;
}

typedef struct known_x86_vender {
	const char *signet;
	int (*cachefxn)(uint32_t,unsigned *);
} known_x86_vender;

static int id_amd_cache(uint32_t,unsigned *);
static int id_via_cache(uint32_t,unsigned *);
static int id_intel_cache(uint32_t,unsigned *);

// There's also: (Collect them all! Impress your friends!)
//      " UMC UMC UMC" "CyriteadxIns" "NexGivenenDr"
//      "RiseRiseRise" "GenuMx86ineT" "Geod NSCe by"
static const known_x86_vender venders[] = {
	{       .signet = "GenuntelineI",
		.cachefxn = id_intel_cache,
	},
	{       .signet = "AuthcAMDenti",
		.cachefxn = id_amd_cache,
	},
	{       .signet = "CentaulsaurH",
		.cachefxn = id_via_cache,
	},
};

// vendstr should be 12 bytes corresponding to EBX, ECX, EDX post-CPUID
static const known_x86_vender *
lookup_vender(const uint32_t *vendstr){
	unsigned z;

	for(z = 0 ; z < sizeof(venders) / sizeof(*venders) ; ++z){
		if(memcmp(vendstr,venders[z].signet,sizeof(*vendstr) * 3) == 0){
			return venders + z;
		}
	}
	return NULL;
}

// By far, the best reference here is the IA-32 Intel Architecture Software
// Developer's Manual. http://faydoc.tripod.com/cpu/cpuid.htm isn't bad, nor
// is http://www.ee.nuigalway.ie/mirrors/www.sandpile.org/ia32/cpuid.htm.
typedef enum {
	CPUID_MAX_SUPPORT		=       0x00000000,
	CPUID_CPU_VERSION		=       0x00000001,
	CPUID_CACHE_TLB		 	=       0x00000002,
	CPUID_EXTENDED_MAX_SUPPORT      =       0x80000000,
	CPUID_EXTENDED_CPU_VERSION      =       0x80000001,
	CPUID_EXTENDED_CPU_NAME1	=       0x80000002,
	CPUID_EXTENDED_CPU_NAME2	=       0x80000003,
	CPUID_EXTENDED_CPU_NAME3	=       0x80000004,
	CPUID_EXTENDED_L1CACHE_TLB      =       0x80000005,
	CPUID_EXTENDED_L2CACHE	  	=       0x80000006,
} cpuid_class;

// Uses all four primary general-purpose 32-bit registers (e[abcd]x), returning
// these in gpregs[0123]. We must preserve EBX ourselves in when -fPIC is used.
static inline void
cpuid(cpuid_class level,uint32_t *gpregs){
#ifdef __x86_64__
	__asm__ __volatile__(
		"cpuid\n\t" // serializing instruction
		: "=a" (gpregs[0]), "=b" (gpregs[1]),
		  "=c" (gpregs[2]), "=d" (gpregs[3])
		: "a" (level)
	);
#else
	__asm__ __volatile__(
		"pushl %%ebx\n\t"
		"cpuid\n\t" // serializing instruction
		"movl %%ebx,%%esi\n\t"
		"popl %%ebx\n\t"
		: "=a" (gpregs[0]), "=S" (gpregs[1]),
		  "=c" (gpregs[2]), "=d" (gpregs[3])
		: "a" (level)
	);
#endif
}

static const known_x86_vender *
identify_extended_cpuid(uint32_t *cpuid_max){
	const known_x86_vender *vender;
	uint32_t gpregs[4];

	cpuid(CPUID_EXTENDED_MAX_SUPPORT,gpregs);
	if((vender = lookup_vender(gpregs + 1)) == NULL){
		return NULL;
	}
	*cpuid_max = gpregs[0];
	return vender;
}

typedef struct intel_dl1_descriptor {
	unsigned descriptor;
	unsigned dl1_line_size;
} intel_dl1_descriptor;

static const intel_dl1_descriptor intel_dl1_descriptors[] = {
	{       .descriptor = 0x0a,
		.dl1_line_size = 32,
	},
	{       .descriptor = 0x0c,
		.dl1_line_size = 32,
	},
	{       .descriptor = 0x2c,
		.dl1_line_size = 64,
	},
	{       .descriptor = 0x60,
		.dl1_line_size = 64,
	},
	{       .descriptor = 0x66,
		.dl1_line_size = 64,
	},
	{       .descriptor = 0x67,
		.dl1_line_size = 64,
	},
	{       .descriptor = 0x68,
		.dl1_line_size = 64,
	},
	{       .descriptor = 0,
		.dl1_line_size = 0,
	}
};

static int
get_intel_clineb(unsigned descriptor,unsigned *clineb){
	const intel_dl1_descriptor *id;

	for(id = intel_dl1_descriptors ; id->descriptor ; ++id){
		if(id->descriptor == descriptor){
			break;
		}
	}
	if(id->descriptor == 0){ // Must periodically add new descriptors :/
		return 0;
	}
	if(*clineb){
		return -1;
	}
	*clineb = id->dl1_line_size;
	return 0;
}

static int
decode_intel_func2(uint32_t *gpregs,unsigned *clineb){
	uint32_t mask;
	unsigned z;

	// Each GP register will set its MSB to 0 if it contains valid 1-byte
	// descriptors in each byte (save AL, the required number of calls).
	for(z = 0 ; z < 4 ; ++z){
		unsigned y;

		if(gpregs[z] & 0x80000000){
			continue;
		}
		mask = 0xff000000;
		for(y = 0 ; y < 4 ; ++y){
			unsigned descriptor;

			if( (descriptor = (gpregs[z] & mask) >> ((3 - y) * 8)) ){
				if(get_intel_clineb(descriptor,clineb)){
					return -1;
				}
			}
			// Don't interpret bits 0..7 of EAX (AL in old notation)
			if((mask >>= 8) == 0x000000ff && z == 0){
				break;
			}
		}
	}
	return 0;
}

// Function 2 of Intel's CPUID -- See 3.1.3 of the CPUID Application Note
static int
extract_intel_func2(unsigned *clineb){
	uint32_t gpregs[4],callreps;
	int ret;

	cpuid(CPUID_CACHE_TLB,gpregs);
	if((callreps = gpregs[0] & 0x000000ff) != 1){
		return -1;
	}
	while((ret = decode_intel_func2(gpregs,clineb)) == 0){
		if(--callreps == 0){
			break;
		}
		cpuid(CPUID_CACHE_TLB,gpregs);
	}
	return ret;
}

static int
id_intel_cache(uint32_t maxlevel,unsigned *clineb){
	if(maxlevel < CPUID_CACHE_TLB){
		return -1;
	}
	*clineb = 0;
	if(extract_intel_func2(clineb)){
		return -1;
	}
	if(*clineb == 0){ // Pentium II's didn't have an L1 cache...
		return -1;
	}
	return 0;
}

static int
id_amd_cache(uint32_t maxlevel __attribute__ ((unused)),unsigned *clineb){
	uint32_t maxexlevel,gpregs[4];
	const known_x86_vender *cpu;

	if((cpu = identify_extended_cpuid(&maxexlevel)) == NULL){
		return -1;
	}
	if(maxexlevel < CPUID_EXTENDED_L1CACHE_TLB){
		return -1;
	}
	// EAX/EBX: 2/4MB / 4KB TLB descriptors ECX: DL1 EDX: CL1
	cpuid(CPUID_EXTENDED_L1CACHE_TLB,gpregs);
	*clineb = gpregs[2] & 0x000000ff;
	return 0;
}

static int
id_via_cache(uint32_t maxlevel __attribute__ ((unused)),unsigned *clineb){
	// FIXME What a cheap piece of garbage, yeargh! VIA doesn't supply
	// cache line info via CPUID. VIA C3 Antaur/Centaur both use 32b. The
	// proof is by method of esoteric reference:
	// http://www.digit-life.com/articles2/rmma/rmma-via-c3.html
	*clineb = 32;
	return 0;
}

// Returns the slot we just added to the end, or NULL on failure.
static inline libtorque_hwmem *
add_hwmem(unsigned *memories,libtorque_hwmem **mems,
		const libtorque_hwmem *amem){
	size_t s = (*memories + 1) * sizeof(**mems);
	typeof(**mems) *tmp;

	if((tmp = realloc(*mems,s)) == NULL){
		return NULL;
	}
	*mems = tmp;
	(*mems)[*memories] = *amem;
	return *mems + (*memories)++;
}

// Before this is called, verify that the CPUID instruction is available via
// receipt of non-zero return from cpuid_available().
int x86cpuid(libtorque_cputype *cpudesc){
	const known_x86_vender *vender;
	libtorque_hwmem mem,*amem;
	uint32_t gpregs[4];

	cpudesc->memories = 0;
	cpudesc->memdescs = NULL;
	cpudesc->elements = 0;
	cpuid(CPUID_MAX_SUPPORT,gpregs);
	if((vender = lookup_vender(gpregs + 1)) == NULL){
		return -1;
	}
	if(vender->cachefxn(gpregs[0],&mem.linesize)){
		return -1;
	}
	mem.totalsize = 0;		// FIXME
	mem.sharedways = 0;		// FIXME
	mem.associativity = 0;		// FIXME
	if((amem = add_hwmem(&cpudesc->memories,&cpudesc->memdescs,&mem)) == NULL){
		return -1;
	}
	return 0;
}
