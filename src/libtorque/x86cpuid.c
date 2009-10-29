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
	int (*memfxn)(uint32_t,libtorque_cputype *);
} known_x86_vender;

static int id_amd_caches(uint32_t,libtorque_cputype *);
static int id_via_caches(uint32_t,libtorque_cputype *);
static int id_intel_caches(uint32_t,libtorque_cputype *);

// There's also: (Collect them all! Impress your friends!)
//      " UMC UMC UMC" "CyriteadxIns" "NexGivenenDr"
//      "RiseRiseRise" "GenuMx86ineT" "Geod NSCe by"
static const known_x86_vender venders[] = {
	{       .signet = "GenuntelineI",
		.memfxn = id_intel_caches,
	},
	{       .signet = "AuthcAMDenti",
		.memfxn = id_amd_caches,
	},
	{       .signet = "CentaulsaurH",
		.memfxn = id_via_caches,
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
	// these are primarily Intel
	CPUID_STANDARD_CPUCONF		=       0x00000002, // sundry cpu data
	CPUID_STANDARD_PSNCONF		=       0x00000003, // processor serial
	CPUID_STANDARD_CACHECONF	=       0x00000004, // cache config
	CPUID_STANDARD_MWAIT		=       0x00000005, // MWAIT/MONITOR
	CPUID_STANDARD_POWERMAN		=       0x00000006, // power management
	CPUID_STANDARD_DIRECTCACHE	=       0x00000009, // DCA access setup
	CPUID_STANDARD_PERFMON		=       0x0000000a, // performance ctrs
	CPUID_STANDARD_TOPOLOGY		=       0x0000000b, // topology config
	CPUID_STANDARD_XSTATE		=       0x0000000d, // XSAVE/XRSTOR
	// these are primarily AMD
	CPUID_EXTENDED_MAX_SUPPORT	=       0x80000000, // max ext. level
	CPUID_EXTENDED_CPU_VERSION	=       0x80000001, // amd cpu sundries
	CPUID_EXTENDED_CPU_NAME1	=       0x80000002, // proc name part1
	CPUID_EXTENDED_CPU_NAME2	=       0x80000003, // proc name part2
	CPUID_EXTENDED_CPU_NAME3	=       0x80000004, // proc name part3
	CPUID_EXTENDED_L1CACHE_TLB	=       0x80000005, // l1 cache, tlb0
	CPUID_EXTENDED_L23CACHE_TLB	=       0x80000006, // l2,3 cache, tlb1
	CPUID_EXTENDED_ENHANCEDPOWER	=       0x80000006, // epm support
} cpuid_class;

// Uses all four primary general-purpose 32-bit registers (e[abcd]x), returning
// these in gpregs[0123]. Secondary parameters are assumed to go in ECX.
static inline void
cpuid(cpuid_class level,uint32_t subparam,uint32_t *gpregs){
	__asm__ __volatile__(
		"cpuid\n\t" // serializing instruction
		: "=&a" (gpregs[0]), "=b" (gpregs[1]),
		  "=&c" (gpregs[2]), "=d" (gpregs[3])
		: "0" (level), "2" (subparam)
	);
}

static inline uint32_t
identify_extended_cpuid(void){
	uint32_t gpregs[4];

	cpuid(CPUID_EXTENDED_MAX_SUPPORT,0,gpregs);
	return gpregs[0];
}

/*typedef struct intel_dc_descriptor {
	unsigned descriptor;
	unsigned linesize;
	unsigned totalsize;
	unsigned associativity;
} intel_dc_descriptor;

static const intel_dc_descriptor intel_dc1_descriptors[] = {
	{       .descriptor = 0x0a,
		.linesize = 32,
		.totalsize = 8 * 1024,
		.associativity = 2,
	},
	{       .descriptor = 0x0c,
		.linesize = 32,
		.totalsize = 16 * 1024,
		.associativity = 4,
	},
	{       .descriptor = 0x2c,
		.linesize = 64,
		.totalsize = 32 * 1024,
		.associativity = 8,
	},
	{       .descriptor = 0x60,
		.linesize = 64,
		.totalsize = 16 * 1024,
		.associativity = 8,
	},
	{       .descriptor = 0x66,
		.linesize = 64,
		.totalsize = 8 * 1024,
		.associativity = 4,
	},
	{       .descriptor = 0x67,
		.linesize = 64,
		.totalsize = 16 * 1024,
		.associativity = 4,
	},
	{       .descriptor = 0x68,
		.linesize = 64,
		.totalsize = 32 * 1024,
		.associativity = 4,
	},
	{       .descriptor = 0,
		.linesize = 0,
		.associativity = 0,
		.totalsize = 0,
	}
};

static const intel_dc_descriptor intel_dc2_descriptors[] = {
	{       .descriptor = 0x1a,
		.linesize = 64,
		.totalsize = 96 * 1024,
		.associativity = 6,
	},
	{       .descriptor = 0x39,
		.linesize = 64,
		.totalsize = 128 * 1024,
		.associativity = 4,
	},
	{       .descriptor = 0x3a,
		.linesize = 64,
		.totalsize = 192 * 1024,
		.associativity = 6,
	},
	{       .descriptor = 0x3b,
		.linesize = 64,
		.totalsize = 128 * 1024,
		.associativity = 2,
	},
	// FIXME many more still
	{       .descriptor = 0,
		.linesize = 0,
		.associativity = 0,
		.totalsize = 0,
	}
};

static int
get_intel_clineb(const intel_dc_descriptor *descs,unsigned descriptor,
				libtorque_hwmem *mem){
	const intel_dc_descriptor *id;

	for(id = descs ; id->descriptor ; ++id){
		if(id->descriptor == descriptor){
			break;
		}
	}
	if(id->descriptor == 0){ // Must keep descriptor tables up to date :/
		return 0;
	}
	if(mem->linesize || mem->totalsize || mem->associativity){
		return -1;
	}
	mem->linesize = id->linesize;
	mem->totalsize = id->totalsize;
	mem->associativity = id->associativity;
	return 0;
}

static int
decode_intel_func2(const intel_dc_descriptor *descs,uint32_t *gpregs,
				libtorque_hwmem *mem){
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
				if(get_intel_clineb(descs,descriptor,mem)){
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
extract_intel_func2(const intel_dc_descriptor *descs,libtorque_hwmem *mem){
	uint32_t gpregs[4],callreps;
	int ret;

	cpuid(CPUID_STANDARD_CPUCONF,0,gpregs);
	if((callreps = gpregs[0] & 0x000000ff) != 1){
		return -1;
	}
	while((ret = decode_intel_func2(descs,gpregs,mem)) == 0){
		if(--callreps == 0){
			break;
		}
		cpuid(CPUID_STANDARD_CPUCONF,0,gpregs);
	}
	return ret;
}

static int
id_intel_caches(uint32_t maxlevel,libtorque_hwmem *mem){
	if(maxlevel < CPUID_STANDARD_CPUCONF){
		return -1;
	}
	memset(mem,0,sizeof(*mem));
	if(extract_intel_func2(intel_dc1_descriptors,mem)){
		return -1;
	}
	if(!mem->linesize || !mem->totalsize || !mem->associativity){
		return -1;
	}
	return 0;
}*/

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

static int
id_intel_caches(uint32_t maxlevel,libtorque_cputype *cpu){
	unsigned n,gotlevel,level,maxdc;
	uint32_t gpregs[4];

	if(maxlevel < CPUID_STANDARD_CACHECONF){
		return -1; // FIXME fall back to CPUID_STANDARD_CPUCONF
	}
	maxdc = level = 0;
	do{
		gotlevel = 0;
		n = 0;
		do{
			enum { NULLCACHE, DATACACHE, CODECACHE, UNIFIEDCACHE } cachet;

			cpuid(CPUID_STANDARD_CACHECONF,n++,gpregs);
			cachet = gpregs[0] & 0x1f; // AX[4..0]
			if(cachet == DATACACHE || cachet == UNIFIEDCACHE){
				unsigned lev = (gpregs[0] >> 5) & 0x7; // AX[7..5]

				if(lev > maxdc){
					maxdc = lev;
				}
				if(lev == level){
					libtorque_hwmem mem;

					if(gotlevel){ // dup d$ descriptors?!
						return -1;
					}
					gotlevel = 1;
					// Linesize is EBX[11:0] + 1
					mem.linesize = (gpregs[1] & 0xfff) + 1;
					// EAX[9]: direct, else (EBX[31..22] + 1)-assoc
					mem.associativity = (gpregs[0] & 0x200) ? 1 :
						(((gpregs[1] >> 22) & 0x3ff) + 1);
					// Partitions = EBX[21:12] + 1, sets = ECX + 1
					mem.totalsize = mem.associativity *
						(((gpregs[1] >> 12) & 0x1ff) + 1) *
						mem.linesize * (gpregs[2] + 1);
					mem.sharedways = ((gpregs[0] >> 14) & 0xfff) + 1;
					if(add_hwmem(&cpu->memories,&cpu->memdescs,&mem) == NULL){
						return -1;
					}
				}
			}
		}while(gpregs[0]);
	}while(++level <= maxdc);
	return level ? 0 : -1;
}

static int
id_amd_caches(uint32_t maxlevel __attribute__ ((unused)),libtorque_cputype *cpud){
	uint32_t maxexlevel,gpregs[4];
	libtorque_hwmem l1amd;

	if((maxexlevel = identify_extended_cpuid()) < CPUID_EXTENDED_L1CACHE_TLB){
		return -1;
	}
	// EAX/EBX: 2/4MB / 4KB TLB descriptors ECX: DL1 EDX: CL1
	cpuid(CPUID_EXTENDED_L1CACHE_TLB,0,gpregs);
	l1amd.linesize = gpregs[2] & 0x000000ff;
	l1amd.associativity = 0; // FIXME
	l1amd.totalsize = 0;	// FIXME
	l1amd.sharedways = 0;	// FIXME
	// FIXME handle other cache levels
	if(add_hwmem(&cpud->memories,&cpud->memdescs,&l1amd) == NULL){
		return -1;
	}
	return 0;
}

static int
id_via_caches(uint32_t maxlevel __attribute__ ((unused)),libtorque_cputype *cpu){
	// FIXME What a cheap piece of garbage, yeargh! VIA doesn't supply
	// cache line info via CPUID. VIA C3 Antaur/Centaur both use 32b. The
	// proof is by method of esoteric reference:
	// http://www.digit-life.com/articles2/rmma/rmma-via-c3.html
	libtorque_hwmem l1via = {
		.linesize = 32,		// FIXME
		.associativity = 0,	// FIXME
		.totalsize = 0,		// FIXME
		.sharedways = 0,	// FIXME
	}; // FIXME handle other levels of cache
	if(add_hwmem(&cpu->memories,&cpu->memdescs,&l1via) == NULL){
		return -1;
	}
	return 0;
}

static int
x86_getbrandname(libtorque_cputype *cpudesc){
	char brandname[16 * 3 + 1]; // each _NAMEx function returns E[BCD]X
	cpuid_class ops[] = { CPUID_EXTENDED_CPU_NAME1,
				CPUID_EXTENDED_CPU_NAME2,
				CPUID_EXTENDED_CPU_NAME3 };
	uint32_t gpregs[4],maxlevel;
	unsigned z;

	if((maxlevel = identify_extended_cpuid()) < CPUID_EXTENDED_CPU_NAME3){
		return -1;
	}
	for(z = 0 ; z < sizeof(ops) / sizeof(*ops) ; ++z){
		unsigned y;

		cpuid(ops[z],0,gpregs);
		for(y = 0 ; y < 4 ; ++y){
			memcpy(&brandname[z * 16 + y * 4],&gpregs[y],sizeof(*gpregs));
		}
	}
	brandname[z * 16] = '\0';
	if((cpudesc->strdescription = strdup(brandname)) == NULL){
		return -1;
	}
	return 0;
}

// Before this is called, verify that the CPUID instruction is available via
// receipt of non-zero return from cpuid_available().
int x86cpuid(libtorque_cputype *cpudesc){
	const known_x86_vender *vender;
	uint32_t gpregs[4];

	cpudesc->memories = 0;
	cpudesc->memdescs = NULL;
	cpudesc->elements = 0;
	cpudesc->apicids = NULL;
	cpudesc->family = cpudesc->stepping = 0;
	cpudesc->extendedsig = cpudesc->model = 0;
	cpuid(CPUID_MAX_SUPPORT,0,gpregs);
	if(x86_getbrandname(cpudesc)){
		return -1;
	}
	if((vender = lookup_vender(gpregs + 1)) == NULL){
		return -1;
	}
	if(vender->memfxn(gpregs[0],cpudesc)){
		return -1;
	}
	return 0;
}
