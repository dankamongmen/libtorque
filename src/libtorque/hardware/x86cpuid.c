#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <libtorque/hardware/arch.h>
#include <libtorque/hardware/x86cpuid.h>

static int
cpuid_available(void){
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

// By far, the best reference here is Intel Application Note 485 (the CPUID
// guide). Also useful are the Intel and AMD Architecture Software Developer's
// Manuals. http://faydoc.tripod.com/cpu/cpuid.htm is pretty useful, as is
// http://www.ee.nuigalway.ie/mirrors/www.sandpile.org/ia32/cpuid.htm.
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
	CPUID_STANDARD_TOPOLOGY		=       0x0000000b, // topology, x2apic
	CPUID_STANDARD_XSTATE		=       0x0000000d, // XSAVE/XRSTOR
	// these are primarily AMD
	CPUID_EXTENDED_MAX_SUPPORT	=       0x80000000, // max ext. level
	CPUID_EXTENDED_CPU_VERSION	=       0x80000001, // amd cpu sundries
	CPUID_EXTENDED_CPU_NAME1	=       0x80000002, // proc name part1
	CPUID_EXTENDED_CPU_NAME2	=       0x80000003, // proc name part2
	CPUID_EXTENDED_CPU_NAME3	=       0x80000004, // proc name part3
	CPUID_AMD_L1CACHE_TLB		=       0x80000005, // l1, tlb0 (AMD)
	CPUID_EXTENDED_L23CACHE_TLB	=       0x80000006, // l2,3, tlb1
	CPUID_EXTENDED_ENHANCEDPOWER	=       0x80000006, // epm support
	CPUID_EXTENDED_TOPOLOGY         =       0x80000008, // topology (AMD)
} cpuid_class;

// Uses all four primary general-purpose 32-bit registers (e[abcd]x), returning
// these in gpregs[0123]. Secondary parameters are assumed to go in ECX.
static inline void
cpuid(cpuid_class level,uint32_t subparam,uint32_t *gpregs){
	__asm__ __volatile__(
#ifdef __x86_64__
		"cpuid\n\t" // serializing instruction
		: "=&a" (gpregs[0]), "=b" (gpregs[1]),
		  "=&c" (gpregs[2]), "=d" (gpregs[3])
		: "0" (level), "2" (subparam)
#else
		"pushl %%ebx\n\t" // can't assume use of ebx on 32-bit
		"cpuid\n\t" // serializing instruction
		"movl %%ebx,%[spill]\n\t"
		"popl %%ebx\n\t"
		: "=&a" (gpregs[0]), [spill] "=r" (gpregs[1]),
		  "=&c" (gpregs[2]), "=d" (gpregs[3])
		: "0" (level), "2" (subparam)
#endif
	);
}

typedef struct known_x86_vendor {
	const char *signet;
	int (*memfxn)(uint32_t,libtorque_cput *);
	int (*topfxn)(uint32_t,libtorque_cput *);
} known_x86_vendor;

static int id_amd_caches(uint32_t,libtorque_cput *);
static int id_via_caches(uint32_t,libtorque_cput *);
static int id_intel_caches(uint32_t,libtorque_cput *);
static int id_amd_topology(uint32_t,libtorque_cput *);
static int id_intel_topology(uint32_t,libtorque_cput *);

// There's also: (Collect them all! Impress your friends!)
//      " UMC UMC UMC" "CyriteadxIns" "NexGivenenDr"
//      "RiseRiseRise" "GenuMx86ineT" "Geod NSCe by"
static const known_x86_vendor vendors[] = {
	{       .signet = "GenuntelineI",
		.memfxn = id_intel_caches,
		.topfxn = id_intel_topology,
	},
	{       .signet = "AuthcAMDenti",
		.memfxn = id_amd_caches,
		.topfxn = id_amd_topology,
	},
	{       .signet = "CentaulsaurH",
		.memfxn = id_via_caches,
		.topfxn = NULL,
	},
};

// vendstr should be 12 bytes corresponding to EBX, ECX, EDX post-CPUID
static const known_x86_vendor *
lookup_vendor(const uint32_t *vendstr){
	unsigned z;

	for(z = 0 ; z < sizeof(vendors) / sizeof(*vendors) ; ++z){
		if(memcmp(vendstr,vendors[z].signet,sizeof(*vendstr) * 3) == 0){
			return vendors + z;
		}
	}
	return NULL;
}

static inline uint32_t
identify_extended_cpuid(void){
	uint32_t gpregs[4];

	cpuid(CPUID_EXTENDED_MAX_SUPPORT,0,gpregs);
	return gpregs[0];
}

typedef struct intel_cache_descriptor {
	unsigned descriptor;
	unsigned linesize;
	unsigned totalsize;
	unsigned associativity;
	unsigned level;
	int memtype;
} intel_cache_descriptor;

static const intel_cache_descriptor intel_cache_descriptors[] = {
	{       .descriptor = 0x06,
		.linesize = 32,
		.totalsize = 8 * 1024,
		.associativity = 4,
		.level = 1,
		.memtype = MEMTYPE_CODE,
	},
	{       .descriptor = 0x08,
		.linesize = 32,
		.totalsize = 16 * 1024,
		.associativity = 4,
		.level = 1,
		.memtype = MEMTYPE_CODE,
	},
	{       .descriptor = 0x09,
		.linesize = 64,
		.totalsize = 32 * 1024,
		.associativity = 4,
		.level = 1,
		.memtype = MEMTYPE_CODE,
	},
	{       .descriptor = 0x0a,
		.linesize = 32,
		.totalsize = 8 * 1024,
		.associativity = 2,
		.level = 1,
		.memtype = MEMTYPE_DATA,
	},
	{       .descriptor = 0x0c,
		.linesize = 32,
		.totalsize = 16 * 1024,
		.associativity = 4,
		.level = 1,
		.memtype = MEMTYPE_DATA,
	},
	{       .descriptor = 0x0d, // ECC
		.linesize = 64,
		.totalsize = 16 * 1024,
		.associativity = 4,
		.level = 1,
		.memtype = MEMTYPE_DATA,
	},
	// IAN 485 describes this as an MLC cache. This doesn't mean
	// Multi-Level Cell (as in NAND flash technology), but
	// "Mid-Level Cache". This essentially means to expect an L3.
	{       .descriptor = 0x21,
		.linesize = 64,
		.totalsize = 256 * 1024,
		.associativity = 8,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x22,
		.linesize = 64,
		.totalsize = 512 * 1024,
		.associativity = 4,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED, // sectored
	},
	{       .descriptor = 0x23,
		.linesize = 64,
		.totalsize = 1024 * 1024,
		.associativity = 8,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED, // sectored
	},
	{       .descriptor = 0x25,
		.linesize = 64,
		.totalsize = 2 * 1024 * 1024,
		.associativity = 8,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED, // sectored
	},
	{       .descriptor = 0x29,
		.linesize = 64,
		.totalsize = 4 * 1024 * 1024,
		.associativity = 8,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED, // sectored
	},
	{       .descriptor = 0x2c,
		.linesize = 64,
		.totalsize = 32 * 1024,
		.associativity = 8,
		.level = 1,
		.memtype = MEMTYPE_DATA, // sectored
	},
	{       .descriptor = 0x30,
		.linesize = 64,
		.totalsize = 32 * 1024,
		.associativity = 8,
		.level = 1,
		.memtype = MEMTYPE_CODE,
	},
	{       .descriptor = 0x39,
		.linesize = 64,
		.totalsize = 128 * 1024,
		.associativity = 4,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED, // sectored
	},
	{       .descriptor = 0x3a,
		.linesize = 64,
		.totalsize = 192 * 1024,
		.associativity = 6,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED, // sectored
	},
	{       .descriptor = 0x3b,
		.linesize = 64,
		.totalsize = 128 * 1024,
		.associativity = 2,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED, // sectored
	},
	{       .descriptor = 0x3c,
		.linesize = 64,
		.totalsize = 256 * 1024,
		.associativity = 4,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED, // sectored
	},
	{       .descriptor = 0x3d,
		.linesize = 64,
		.totalsize = 384 * 1024,
		.associativity = 6,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED, // sectored
	},
	{       .descriptor = 0x3e,
		.linesize = 64,
		.totalsize = 512 * 1024,
		.associativity = 4,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED, // sectored
	},
	{       .descriptor = 0x41,
		.linesize = 32,
		.totalsize = 128 * 1024,
		.associativity = 4,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x42,
		.linesize = 32,
		.totalsize = 256 * 1024,
		.associativity = 4,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x43,
		.linesize = 32,
		.totalsize = 512 * 1024,
		.associativity = 4,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x44,
		.linesize = 32,
		.totalsize = 1024 * 1024,
		.associativity = 4,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x45,
		.linesize = 32,
		.totalsize = 2 * 1024 * 1024,
		.associativity = 4,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x46,
		.linesize = 64,
		.totalsize = 4 * 1024 * 1024,
		.associativity = 4,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x47,
		.linesize = 64,
		.totalsize = 8 * 1024 * 1024,
		.associativity = 8,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x48, // unified on-die
		.linesize = 64,
		.totalsize = 3 * 1024 * 1024,
		.associativity = 12,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x49, 		// FIXME has two meanings!
		.linesize = 64,
		.totalsize = 4 * 1024 * 1024,
		.associativity = 16,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x4a,
		.linesize = 64,
		.totalsize = 6 * 1024 * 1024,
		.associativity = 12,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x4b,
		.linesize = 64,
		.totalsize = 8 * 1024 * 1024,
		.associativity = 16,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x4c,
		.linesize = 64,
		.totalsize = 12 * 1024 * 1024,
		.associativity = 12,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x4d,
		.linesize = 64,
		.totalsize = 16 * 1024 * 1024,
		.associativity = 16,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x4e,
		.linesize = 64,
		.totalsize = 6 * 1024 * 1024,
		.associativity = 24,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x60,
		.linesize = 64,
		.totalsize = 16 * 1024,
		.associativity = 8,
		.level = 1,
		.memtype = MEMTYPE_DATA,
	},
	{       .descriptor = 0x66,
		.linesize = 64,
		.totalsize = 8 * 1024,
		.associativity = 4,
		.level = 1,
		.memtype = MEMTYPE_DATA,
	},
	{       .descriptor = 0x67,
		.linesize = 64,
		.totalsize = 16 * 1024,
		.associativity = 4,
		.level = 1,
		.memtype = MEMTYPE_DATA,
	},
	{       .descriptor = 0x68,
		.linesize = 64,
		.totalsize = 32 * 1024,
		.associativity = 4,
		.level = 1,
		.memtype = MEMTYPE_DATA,
	},
	{       .descriptor = 0x7a, // sectored
		.linesize = 64,
		.totalsize = 256 * 1024,
		.associativity = 8,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x7b, // sectored
		.linesize = 64,
		.totalsize = 512 * 1024,
		.associativity = 8,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x7c, // sectored
		.linesize = 64,
		.totalsize = 1 * 1024 * 1024,
		.associativity = 8,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x7d,
		.linesize = 64,
		.totalsize = 2 * 1024 * 1024,
		.associativity = 8,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x7f,
		.linesize = 64,
		.totalsize = 512 * 1024,
		.associativity = 2,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x82,
		.linesize = 32,
		.totalsize = 256 * 1024,
		.associativity = 8,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x83,
		.linesize = 32,
		.totalsize = 512 * 1024,
		.associativity = 8,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x84,
		.linesize = 32,
		.totalsize = 1 * 1024 * 1024,
		.associativity = 8,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x85,
		.linesize = 32,
		.totalsize = 2 * 1024 * 1024,
		.associativity = 8,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x86,
		.linesize = 64,
		.totalsize = 512 * 1024,
		.associativity = 4,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0x87,
		.linesize = 64,
		.totalsize = 1 * 1024,
		.associativity = 8,
		.level = 2,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0xd0,
		.linesize = 64,
		.totalsize = 512 * 1024,
		.associativity = 4,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0xd1,
		.linesize = 64,
		.totalsize = 1024 * 1024,
		.associativity = 4,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0xd2,
		.linesize = 64,
		.totalsize = 2 * 1024 * 1024,
		.associativity = 4,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0xd6,
		.linesize = 64,
		.totalsize = 12 * 1024 * 1024,
		.associativity = 8,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0xd7,
		.linesize = 64,
		.totalsize = 18 * 1024 * 1024,
		.associativity = 8,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0xd8,
		.linesize = 64,
		.totalsize = 24 * 1024 * 1024,
		.associativity = 8,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0xdc,
		.linesize = 64,
		.totalsize = (1024 + 512) * 1024,
		.associativity = 12,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0xdd,
		.linesize = 64,
		.totalsize = 3 * 1024 * 1024,
		.associativity = 12,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0xde,
		.linesize = 64,
		.totalsize = 6 * 1024 * 1024,
		.associativity = 12,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0xe2,
		.linesize = 64,
		.totalsize = 2 * 1024 * 1024,
		.associativity = 16,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0xe3,
		.linesize = 64,
		.totalsize = 4 * 1024 * 1024,
		.associativity = 16,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0xe4,
		.linesize = 64,
		.totalsize = 8 * 1024 * 1024,
		.associativity = 16,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0xea,
		.linesize = 64,
		.totalsize = 12 * 1024 * 1024,
		.associativity = 24,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0xeb,
		.linesize = 64,
		.totalsize = 18 * 1024 * 1024,
		.associativity = 24,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
	{       .descriptor = 0xec,
		.linesize = 64,
		.totalsize = 24 * 1024 * 1024,
		.associativity = 24,
		.level = 3,
		.memtype = MEMTYPE_UNIFIED,
	},
};

typedef struct intel_tlb_descriptor {
	unsigned descriptor;
	unsigned pagesize;
	unsigned entries;
	unsigned associativity;
	unsigned level;
	int tlbtype;
} intel_tlb_descriptor;

static const intel_tlb_descriptor intel_tlb_descriptors[] = {
	{	.descriptor = 0x01,
		.pagesize = 4 * 1024,
		.entries = 32,
		.associativity = 4,
		.level = 2,
		.tlbtype = MEMTYPE_CODE,
	},
	{	.descriptor = 0x02,
		.pagesize = 4 * 1024 * 1024,
		.entries = 2,
		.associativity = 2,
		.level = 2,
		.tlbtype = MEMTYPE_CODE,
	},
	{	.descriptor = 0x03,
		.pagesize = 4 * 1024,
		.entries = 64,
		.associativity = 4,
		.level = 2,
		.tlbtype = MEMTYPE_DATA,
	},
	{	.descriptor = 0x04,
		.pagesize = 4 * 1024 * 1024,
		.entries = 8,
		.associativity = 4,
		.level = 2,
		.tlbtype = MEMTYPE_DATA,
	},
	{	.descriptor = 0x05,
		.pagesize = 4 * 1024 * 1024,
		.entries = 32,
		.associativity = 4,
		.level = 2,
		.tlbtype = MEMTYPE_DATA,
	},
	{	.descriptor = 0x50,
		.pagesize = 4 * 1024 * 1024,	// FIXME 4K, 2M or 4M
		.entries = 64,
		.associativity = 64,
		.level = 1,
		.tlbtype = MEMTYPE_CODE,
	},
	{	.descriptor = 0x51,
		.pagesize = 4 * 1024,		// FIXME 4K, 2M or 4M
		.entries = 128,
		.associativity = 128,
		.level = 1,
		.tlbtype = MEMTYPE_CODE,
	},
	{	.descriptor = 0x52,
		.pagesize = 4 * 1024 * 1024,	// FIXME 4K, 2M or 4M
		.entries = 256,
		.associativity = 256,
		.level = 1,
		.tlbtype = MEMTYPE_CODE,
	},
	{	.descriptor = 0x55,
		.pagesize = 4 * 1024,		// FIXME 2M or 4M
		.entries = 7,
		.associativity = 7,
		.level = 2,
		.tlbtype = MEMTYPE_CODE,
	},
	{	.descriptor = 0x56,
		.pagesize = 4 * 1024 * 1024,
		.entries = 16,
		.associativity = 4,
		.level = 1,
		.tlbtype = MEMTYPE_DATA,
	},
	{	.descriptor = 0x57,
		.pagesize = 4 * 1024,
		.entries = 16,
		.associativity = 4,
		.level = 1,
		.tlbtype = MEMTYPE_DATA,
	},
	{	.descriptor = 0x5a,		// FIXME 2M or 4M
		.pagesize = 4 * 1024 * 1024,
		.entries = 32,
		.associativity = 4,
		.level = 1,
		.tlbtype = MEMTYPE_DATA,
	},
	{	.descriptor = 0x5b,
		.pagesize = 4 * 1024,		// FIXME 4k or 4M
		.entries = 64,
		.associativity = 64,
		.level = 2,
		.tlbtype = MEMTYPE_DATA,
	},
	{	.descriptor = 0x5c,
		.pagesize = 4 * 1024,		// FIXME 4k or 4M
		.entries = 128,
		.associativity = 128,
		.level = 2,
		.tlbtype = MEMTYPE_DATA,
	},
	{	.descriptor = 0x5d,
		.pagesize = 4 * 1024,		// FIXME 4k or 4M
		.entries = 256,
		.associativity = 256,
		.level = 2,
		.tlbtype = MEMTYPE_DATA,
	},
	{	.descriptor = 0xb0,
		.pagesize = 4 * 1024,
		.entries = 128,
		.associativity = 4,
		.level = 1,
		.tlbtype = MEMTYPE_CODE,
	},
	{	.descriptor = 0xb1,		// FIXME 8x2M or 4x4M
		.pagesize = 2 * 1024 * 1024,
		.entries = 8,
		.associativity = 4,
		.level = 1,
		.tlbtype = MEMTYPE_CODE,
	},
	{	.descriptor = 0xb2,
		.pagesize = 4 * 1024,
		.entries = 64,
		.associativity = 4,
		.level = 1,
		.tlbtype = MEMTYPE_CODE,
	},
	{	.descriptor = 0xb3,
		.pagesize = 4 * 1024,
		.entries = 128,
		.associativity = 4,
		.level = 2,
		.tlbtype = MEMTYPE_DATA,
	},
	{	.descriptor = 0xb4,
		.pagesize = 4 * 1024,
		.entries = 256,
		.associativity = 4,
		.level = 2,
		.tlbtype = MEMTYPE_DATA,
	},
	{	.descriptor = 0xca,
		.pagesize = 4 * 1024,
		.entries = 512,
		.associativity = 4,
		.level = 2,
		.tlbtype = MEMTYPE_DATA,
	},
};

static const unsigned intel_trace_descriptors[] = {
	0x70, // 12k uops, 8-way
	0x71, // 16k uops, 8-way
	0x72, // 32k uops, 8-way
	0x73, // 64K uops, 8-way
};

// Returns the slot we just added to the end, or NULL on failure.
static inline libtorque_memt *
add_hwmem(unsigned *memories,libtorque_memt **mems,
		const libtorque_memt *amem){
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
get_intel_cache(unsigned descriptor,libtorque_memt *mem,unsigned sharedways){
	unsigned n;

	// FIXME convert this to a table indexed by (8-bit) descriptor
	for(n = 0 ; n < sizeof(intel_cache_descriptors) / sizeof(*intel_cache_descriptors) ; ++n){
		if(intel_cache_descriptors[n].descriptor == descriptor){
			mem->memtype = intel_cache_descriptors[n].memtype;
			mem->linesize = intel_cache_descriptors[n].linesize;
			mem->totalsize = intel_cache_descriptors[n].totalsize;
			mem->associativity = intel_cache_descriptors[n].associativity;
			mem->level = intel_cache_descriptors[n].level;
			mem->sharedways = sharedways;
			return 0;
		}
	}
	return -1;
}

static int
get_intel_tlb(unsigned descriptor,libtorque_tlbt *tlb,unsigned sharedways){
	unsigned n;

	for(n = 0 ; n < sizeof(intel_tlb_descriptors) / sizeof(*intel_tlb_descriptors) ; ++n){
		if(intel_tlb_descriptors[n].descriptor == descriptor){
			tlb->pagesize = intel_tlb_descriptors[n].pagesize;
			tlb->entries = intel_tlb_descriptors[n].entries;
			tlb->associativity = intel_tlb_descriptors[n].associativity;
			tlb->tlbtype = intel_tlb_descriptors[n].tlbtype;
			tlb->level = intel_tlb_descriptors[n].level;
			tlb->sharedways = sharedways;
			return 0;
		}
	}
	return -1;
}

static int
get_intel_trace(unsigned descriptor){
	unsigned n;

	for(n = 0 ; n < sizeof(intel_trace_descriptors) / sizeof(*intel_trace_descriptors) ; ++n){
		if(intel_trace_descriptors[n] == descriptor){
			return 0;
		}
	}
	return -1;
}

static libtorque_tlbt *
add_tlb(unsigned *tlbs,libtorque_tlbt **tlbdescs,const libtorque_tlbt *tlb){
	size_t s = (*tlbs + 1) * sizeof(**tlbdescs);
	typeof(**tlbdescs) *tmp;

	if((tmp = realloc(*tlbdescs,s)) == NULL){
		return NULL;
	}
	*tlbdescs = tmp;
	(*tlbdescs)[*tlbs] = *tlb;
	return *tlbdescs + (*tlbs)++;
}

static inline int
compare_memdetails(const libtorque_memt * restrict a,
			const libtorque_memt * restrict b){
	// See match_memtype(); do not evaluate sharing for equality!
	if(a->totalsize != b->totalsize || a->associativity != b->associativity ||
			a->level != b->level || a->linesize != b->linesize ||
			a->memtype != b->memtype){
		return -1;
	}
	return 0;
}

// *DOES NOT* compare sharing values, since that isn't yet generally detected
// at memory detection time.
static libtorque_memt *
match_memtype(unsigned memtc,libtorque_memt *types,
		const libtorque_memt *amem){
	unsigned n;

	for(n = 0 ; n < memtc ; ++n){
		if(compare_memdetails(types + n,amem) == 0){
			return types + n;
		}
	}
	return NULL;
}

static int
decode_intel_func2(libtorque_cput *cpu,uint32_t *gpregs){
	uint32_t mask;
	unsigned z;

	// Each GP register will set its MSB to 0 if it contains valid 1-byte
	// descriptors in each byte (save AL, the required number of calls).
	for(z = 0 ; z < 4 ; ++z){
		unsigned y;

		if(gpregs[z] & 0x80000000u){
			continue;
		}
		mask = 0xff000000;
		for(y = 0 ; y < 4 ; ++y){
			unsigned descriptor;

			if( (descriptor = (gpregs[z] & mask) >> ((3 - y) * 8u)) ){
				libtorque_memt mem;
				libtorque_tlbt tlb;

				// Physical resources are shared at least by
				// the logical cores, but also further FIXME
				if(get_intel_cache(descriptor,&mem,cpu->threadspercore) == 0){
					// Don't add duplicates from CPUID Fxn4
					if(!match_memtype(cpu->memories,cpu->memdescs,&mem)){
						if(add_hwmem(&cpu->memories,&cpu->memdescs,&mem) == NULL){
							return -1;
						}
					}
				}else if(get_intel_tlb(descriptor,&tlb,cpu->threadspercore) == 0){
					if(add_tlb(&cpu->tlbs,&cpu->tlbdescs,&tlb) == NULL){
						return -1;
					}
				}else if(get_intel_trace(descriptor) == 0){
					// no one cares
				}else if(descriptor == 0xf0){
					// FIXME 64-byte prefetching
				}else if(descriptor == 0xf1){
					// FIXME 128-byte prefetching
				}else if(descriptor == 0x40){
					// Means "no higher(?)-level cache"
				}else{
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
id_intel_caches_old(uint32_t maxlevel,libtorque_cput *cpu){
	uint32_t gpregs[4],callreps;
	int ret;

	if(maxlevel < CPUID_STANDARD_CPUCONF){
		return -1;
	}
	cpuid(CPUID_STANDARD_CPUCONF,0,gpregs);
	if((callreps = gpregs[0] & 0x000000ffu) != 1){
		return -1;
	}
	while(!(ret = decode_intel_func2(cpu,gpregs))){
		if(--callreps == 0){
			break;
		}
		cpuid(CPUID_STANDARD_CPUCONF,0,gpregs);
	}
	return ret;
}

static int
id_intel_caches(uint32_t maxlevel,libtorque_cput *cpu){
	unsigned n,level,maxdc;
	uint32_t gpregs[4];

	if(maxlevel < CPUID_STANDARD_CACHECONF){
		// We determine the number of cores per package using the
		// deterministic cache function (for some reason). Thankfully,
		// all multicore processors support said function.
		cpu->coresperpackage = 1;
		return id_intel_caches_old(maxlevel,cpu);
	}
	maxdc = level = 1;
	do{
		enum { // Table 2.9, IAN 485
			NULLCACHE = 0,
			DATACACHE = 1,
			CODECACHE = 2,
			UNIFIEDCACHE = 3,
		} cachet;
		n = 0;
		do{
			libtorque_memt mem;
			unsigned lev,cpp;

			cpuid(CPUID_STANDARD_CACHECONF,n++,gpregs);
			lev = (gpregs[0] >> 5) & 0x7u; // AX[7..5]
			cachet = gpregs[0] & 0x1fu; // AX[4..0]
			if(cachet == DATACACHE){ // Memory type is in AX[4..0]
				mem.memtype = MEMTYPE_DATA;
			}else if(cachet == CODECACHE){
				mem.memtype = MEMTYPE_CODE;
			}else if(cachet == UNIFIEDCACHE){
				mem.memtype = MEMTYPE_UNIFIED;
			}else if(cachet == NULLCACHE){
				continue;
			}else{
				return -1;
			}
			if(lev > maxdc){
				maxdc = lev;
			}
			if(lev != level){
				continue;
			}
			// Linesize is EBX[11:0] + 1
			mem.linesize = (gpregs[1] & 0xfffu) + 1;
			// EAX[9]: direct, else (EBX[31..22] + 1)-assoc
			mem.associativity = (gpregs[0] & 0x200u) ? 1 :
				(((gpregs[1] >> 22) & 0x3ffu) + 1);
			// Partitions = EBX[21:12] + 1, sets = ECX + 1
			mem.totalsize = mem.associativity *
				(((gpregs[1] >> 12) & 0x1ffu) + 1) *
				mem.linesize * (gpregs[2] + 1);
			mem.sharedways = ((gpregs[0] >> 14) & 0xfffu) + 1;
			mem.level = lev;
			// Cores per package = EAX[31:26] + 1
			if((cpp = ((gpregs[0] >> 26) & 0x3fu) + 1) == 0){
				return -1;
			}
			if(cpu->coresperpackage == 0){
				if(maxlevel < CPUID_STANDARD_TOPOLOGY){
					// See comments within x86_getprocsig()
					if((cpu->threadspercore /= cpp) == 0){
						return -1;
					}
				}
				cpu->coresperpackage = cpp;
			}else if(cpu->coresperpackage != cpp){
				return -1;
			}
			if(mem.sharedways < cpu->threadspercore){
				mem.sharedways = cpu->threadspercore;
			}
			if(add_hwmem(&cpu->memories,&cpu->memdescs,&mem) == NULL){
				return -1;
			}
		}while(cachet != NULLCACHE);
	}while(++level <= maxdc);
	return id_intel_caches_old(maxlevel,cpu);
}

// FIXME we need to check the presence of either/or; right now, we break if
// only one is present (because later we try to add both)
static inline unsigned
amd_tlb_presentp(uint32_t reg){
	return (((reg >> 12u) & 0xf) || (reg >> 28u));
}

static inline unsigned
amd_cache_presentp(uint32_t reg){
	return !!((reg >> 12u) & 0xf);
}

static unsigned
amd_l23assoc(unsigned idx,unsigned lines){
	switch(idx){
		case 0xf: return lines; // fully associative
		case 0xe: return 128;
		case 0xd: return 96;
		case 0xc: return 64;
		case 0xb: return 48;
		case 0xa: return 32;
		case 0x8: return 16;
		case 0x6: return 8;
		case 0x4: return 4;
		case 0x2: return 2;
		case 0x1: return 1;
		// 0 == explicitly disabled. all other values reserved.
	}
	return 0;
}

static int
decode_amd_l23tlb(uint32_t reg,unsigned *dassoc,unsigned *iassoc,unsigned *dents,
					unsigned *ients){
	*dents = (reg >> 16u) & 0xfffu;
	*ients = reg & 0xfffu;
	*dassoc = amd_l23assoc(reg >> 28u,*dents);
	*iassoc = amd_l23assoc((reg >> 12u) & 0xf,*ients);
	return (*dents && *ients && *dassoc && *iassoc) ? 0 : -1;
}

static int
decode_amd_l23cache(uint32_t reg,unsigned *size,unsigned *assoc,unsigned *lsize,
			unsigned shift,unsigned mul){
	*size = (reg >> shift) * 1024 * mul;
	*lsize = reg & 0xffu;
	*assoc = amd_l23assoc((reg >> 12u) & 0xf,*size / *lsize);
	return (*size && *assoc && *lsize) ? 0 : -1;
}

static int
id_amd_23caches(uint32_t maxexlevel,uint32_t *gpregs,libtorque_cput *cpud){
	libtorque_tlbt tlb,tlb24,itlb,itlb24;
	libtorque_memt l2cache,l3cache;

	if(maxexlevel < CPUID_EXTENDED_L23CACHE_TLB){
		return 0;
	}
	cpuid(CPUID_EXTENDED_L23CACHE_TLB,0,gpregs);
	l2cache.sharedways = cpud->threadspercore;
	l3cache.sharedways = cpud->coresperpackage;
	l2cache.level = 2;
	l3cache.level = 3;
	l2cache.memtype = l3cache.memtype = MEMTYPE_UNIFIED;
	if(amd_cache_presentp(gpregs[2])){
		if(decode_amd_l23cache(gpregs[2],&l2cache.totalsize,&l2cache.associativity,
					&l2cache.linesize,16,1)){
			return -1;
		}
		if(add_hwmem(&cpud->memories,&cpud->memdescs,&l2cache) == NULL){
			return -1;
		}
	}
	if(amd_cache_presentp(gpregs[3])){
		if(decode_amd_l23cache(gpregs[3],&l3cache.totalsize,&l3cache.associativity,
					&l3cache.linesize,18,512)){
			return -1;
		}
		if(add_hwmem(&cpud->memories,&cpud->memdescs,&l3cache) == NULL){
			return -1;
		}
	}
	tlb.pagesize = itlb.pagesize = 4096;
	tlb24.pagesize = itlb24.pagesize = 1024 * 2096; // FIXME
	tlb.sharedways = itlb.sharedways = cpud->threadspercore;
	tlb24.sharedways = itlb24.sharedways = cpud->threadspercore;
	tlb.level = itlb.level = tlb24.level = itlb24.level = 2;
	tlb.tlbtype = tlb24.tlbtype = MEMTYPE_DATA;
	itlb.tlbtype = itlb24.tlbtype = MEMTYPE_CODE;
	if(amd_tlb_presentp(gpregs[0])){
		if(decode_amd_l23tlb(gpregs[0],&tlb24.associativity,&itlb24.associativity,
					&tlb24.entries,&itlb24.entries)){
			return -1;
		}
		if(add_tlb(&cpud->tlbs,&cpud->tlbdescs,&tlb) == NULL){
			return -1;
		}
		if(add_tlb(&cpud->tlbs,&cpud->tlbdescs,&itlb) == NULL){
			return -1;
		}
	}
	if(amd_tlb_presentp(gpregs[1])){
		if(decode_amd_l23tlb(gpregs[1],&tlb.associativity,&itlb.associativity,
					&tlb.entries,&itlb.entries)){
			return -1;
		}
		if(add_tlb(&cpud->tlbs,&cpud->tlbdescs,&tlb24) == NULL){
			return -1;
		}
		if(add_tlb(&cpud->tlbs,&cpud->tlbdescs,&itlb24) == NULL){
			return -1;
		}
	}
	return 0;
}

static int
decode_amd_l1tlb(uint32_t reg,unsigned *dassoc,unsigned *iassoc,unsigned *dents,
					unsigned *ients){
	*dassoc = reg >> 24u;
	*iassoc = (reg >> 8u) & 0xffu;
	*dents = (reg >> 16u) & 0xffu;
	*ients = reg & 0xffu;
	return (*dassoc && *iassoc && *dents && *ients) ? 0 : -1;
}

static int
decode_amd_l1cache(uint32_t reg,unsigned *size,unsigned *assoc,unsigned *lsize){
	*size = (reg >> 24u) * 1024u;
	*assoc = (reg >> 16u) & 0xffu;
	*lsize = reg & 0xffu;
	return (*size && *assoc && *lsize) ? 0 : -1;
}

static int
id_amd_caches(uint32_t maxlevel __attribute__ ((unused)),libtorque_cput *cpud){
	libtorque_tlbt tlb,tlb24,itlb,itlb24;
	libtorque_memt l1dcache,l1icache;
	uint32_t maxex,gpregs[4];

	if((maxex = identify_extended_cpuid()) < CPUID_AMD_L1CACHE_TLB){
		return -1;
	}
	// EAX/EBX: 2/4MB / 4KB TLB descriptors ECX: DL1 EDX: CL1
	cpuid(CPUID_AMD_L1CACHE_TLB,0,gpregs);
	if(decode_amd_l1cache(gpregs[2],&l1icache.totalsize,&l1icache.associativity,
				&l1icache.linesize)){
		return -1;
	}
	if(decode_amd_l1cache(gpregs[3],&l1dcache.totalsize,&l1dcache.associativity,
				&l1dcache.linesize)){
		return -1;
	}
	l1icache.sharedways = l1dcache.sharedways = cpud->threadspercore;
	l1icache.level = l1dcache.level = 1;
	l1icache.memtype = MEMTYPE_CODE;
	l1dcache.memtype = MEMTYPE_DATA;
	if(decode_amd_l1tlb(gpregs[0],&tlb24.associativity,&itlb24.associativity,
				&tlb24.entries,&itlb24.entries)){
		return -1;
	}
	if(decode_amd_l1tlb(gpregs[1],&tlb.associativity,&itlb.associativity,
				&tlb.entries,&itlb.entries)){
		return -1;
	}
	tlb.pagesize = itlb.pagesize = 4096;
	tlb24.pagesize = itlb24.pagesize = 1024 * 2096; // FIXME
	tlb.sharedways = itlb.sharedways = cpud->threadspercore;
	tlb24.sharedways = itlb24.sharedways = cpud->threadspercore;
	tlb.level = itlb.level = tlb24.level = itlb24.level = 1;
	tlb.tlbtype = tlb24.tlbtype = MEMTYPE_DATA;
	itlb.tlbtype = itlb24.tlbtype = MEMTYPE_CODE;
	if(add_hwmem(&cpud->memories,&cpud->memdescs,&l1icache) == NULL){
		return -1;
	}
	if(add_hwmem(&cpud->memories,&cpud->memdescs,&l1dcache) == NULL){
		return -1;
	}
	if(add_tlb(&cpud->tlbs,&cpud->tlbdescs,&tlb) == NULL){
		return -1;
	}
	if(add_tlb(&cpud->tlbs,&cpud->tlbdescs,&tlb24) == NULL){
		return -1;
	}
	if(add_tlb(&cpud->tlbs,&cpud->tlbdescs,&itlb) == NULL){
		return -1;
	}
	if(add_tlb(&cpud->tlbs,&cpud->tlbdescs,&itlb24) == NULL){
		return -1;
	}
	if(id_amd_23caches(maxex,gpregs,cpud)){
		return -1;
	}
	return 0;
}

static int
id_via_caches(uint32_t maxlevel __attribute__ ((unused)),libtorque_cput *cpu){
	// FIXME What a cheap piece of garbage, yeargh! VIA doesn't supply
	// cache line info via CPUID. VIA C3 Antaur/Centaur both use 32b. The
	// proof is by method of esoteric reference:
	// http://www.digit-life.com/articles2/rmma/rmma-via-c3.html
	libtorque_memt l1via = {
		.level = 1,
		.linesize = 32,			// FIXME
		.associativity = 0,		// FIXME
		.totalsize = 0,			// FIXME
		.sharedways = 0,		// FIXME
		.memtype = MEMTYPE_UNKNOWN,	// FIXME
	}; // FIXME handle other levels of cache
	if(add_hwmem(&cpu->memories,&cpu->memdescs,&l1via) == NULL){
		return -1;
	}
	return 0;
}

static int
x86_getbrandname(libtorque_cput *cpudesc){
	char *aname,brandname[16 * 3 + 1]; // _NAMEx functions return E[BCD]X
	cpuid_class ops[] = { CPUID_EXTENDED_CPU_NAME1,
				CPUID_EXTENDED_CPU_NAME2,
				CPUID_EXTENDED_CPU_NAME3 };
	uint32_t maxlevel;
	int hadspace;
	unsigned z;

	if((maxlevel = identify_extended_cpuid()) < CPUID_EXTENDED_CPU_NAME3){
		return -1;
	}
	// We want to remove duplicate and leading spaces (not localized white-
	// space, but ASCII 0x20 (SP). Do *not* use isspace(), etc).
	hadspace = 1;
	aname = brandname;
	for(z = 0 ; z < sizeof(ops) / sizeof(*ops) ; ++z){
		uint32_t gpregs[4];
		unsigned y;

		cpuid(ops[z],0,gpregs);
		for(y = 0 ; y < sizeof(gpregs) / sizeof(*gpregs) ; ++y){
			unsigned x;

			for(x = 0 ; x < 4 ; ++x){
				char c = ((const char *)(gpregs + y))[x];

				if(c != ' ' || !hadspace){
					*aname++ = c;
					hadspace = c == ' ';
				}
			}
		}
	}
	*aname = '\0';
	if((cpudesc->strdescription = strdup(brandname)) == NULL){
		return -1;
	}
	return 0;
}

static int
id_x86_topology(uint32_t maxfunc,libtorque_cput *cpu){
	uint32_t gpregs[4];

	if(maxfunc < CPUID_CPU_VERSION){
		return -1;
	}
	cpuid(CPUID_CPU_VERSION,0,gpregs);
	// http://software.intel.com/en-us/articles/intel-64-architecture-processor-topology-enumeration/
	// EBX[23..16] is "the maximum number of addressable ID's that can be
	// assigned to logical processors in a physical package." Also: "For
	// processors that report CPUID.1:EBX[23:16] as reserved (i.e. 0), the
	// processor supports only one level of topology." EBX[23:16] on AMD is
	// "LogicalProcessorCount", *iff* CPUID1.EDX[HTT] is 1 FIXME.
	if((cpu->threadspercore = (gpregs[1] >> 16) & 0xffu) == 0){
		cpu->threadspercore = 1; // can't have 0 threads
	}
	// Round it to the nearest >= power of 2...
	while(cpu->threadspercore & (cpu->threadspercore - 1)){
		++cpu->threadspercore;
	}
	// ...then divide by EAX[31:26] with CPUID_STANDARD_CACHECONF. We
	// do this in id_intel_caches(), if it's available.
	return 0;
}

static int
id_amd_topology(uint32_t maxfunc,libtorque_cput *cpu){
	return id_x86_topology(maxfunc,cpu);
}

static int
id_intel_topology(uint32_t maxfunc,libtorque_cput *cpu){
	uint32_t gpregs[4];

	if(maxfunc < CPUID_STANDARD_TOPOLOGY){
		return id_x86_topology(maxfunc,cpu);
	}
	cpuid(CPUID_STANDARD_TOPOLOGY,0,gpregs);
	if(((gpregs[2] >> 8) & 0xffu) != 1u){
		return -1;
	}
	if((cpu->threadspercore = (gpregs[1] & 0xffffu)) == 0){
		return -1;
	}
	cpuid(CPUID_STANDARD_TOPOLOGY,1,gpregs);
	if(((gpregs[2] >> 8) & 0xffu) != 2u){
		return -1;
	}
	if((cpu->coresperpackage = (gpregs[1] & 0xffffu)) == 0){
		return -1;
	}
	return 0;
}

static int
x86_getprocsig(uint32_t maxfunc,libtorque_cput *cpu){
	uint32_t gpregs[4];

	if(maxfunc < CPUID_CPU_VERSION){
		return -1;
	} // CPUID1.EAX is the same on Intel and AMD
	cpuid(CPUID_CPU_VERSION,0,gpregs);
	cpu->stepping = gpregs[0] & 0xfu; // Stepping: EAX[3..0]
	cpu->x86type = (gpregs[0] >> 12) & 0x2u; // Processor type: EAX[13..12]
	// Extended model is EAX[19..16]. Model is EAX[7..4].
	cpu->model = ((gpregs[0] >> 12) & 0xf0u) | ((gpregs[0] >> 4) & 0xfu);
	// Extended family is EAX[27..20]. Family is EAX[11..8].
	cpu->family = ((gpregs[0] >> 17) & 0x7f8u) | ((gpregs[0] >> 8) & 0xfu);
	return 0;
}

static inline int
x86apic(unsigned maxlevel,uint32_t *apic){
	uint32_t gpregs[4],lev;

	// CPUID1.EBX[31:24] is the local APIC on Intel and AMD
	if(maxlevel < CPUID_CPU_VERSION){
		return -1; // FIXME any other way to get local APIC?
	}
	cpuid(CPUID_CPU_VERSION,0,gpregs);
	*apic = (gpregs[1] >> 24) & 0xffu; 	// 8-bit legacy APIC
	// AMD doesn't have extended APIC as of 25481-Revision 2.28 FIXME
	if(maxlevel < CPUID_STANDARD_TOPOLOGY){ // We only have legacy APIC
		return 0;
	}
	cpuid(CPUID_STANDARD_TOPOLOGY,0,gpregs);
	// EDX holds the 32-bit Extended APIC. Last 8 bits ought equal legacy.
	if((gpregs[3] & 0xff) != *apic){
		return -1;
	}
	*apic = gpregs[3];
	// ECX[15..8] holds "Level type": 0 == invalid, 1 == thread, 2 == core
	while( (lev = (gpregs[2] >> 8) & 0xffu) ){
		switch(lev){
			case 0x2: // core
				break;
			case 0x1: // thread
				break;
			default:
				return -1;
		}
		cpuid(CPUID_STANDARD_TOPOLOGY,++lev,gpregs);
		if(gpregs[3] != *apic){
			return -1;
		}
	}
	return 0;
}

static int
x86topology(unsigned maxlevel,const libtorque_cput *cpu,unsigned *thread,
				unsigned *core,unsigned *pkg){
	unsigned tpc,cpp;
	uint32_t apic;

	*core = 0;
	*thread = 0;
	if(x86apic(maxlevel,&apic)){
		return -1;
	}
	if((tpc = cpu->threadspercore) == 0){
		return -1;
	}
	if((cpp = cpu->coresperpackage) == 0){
		return -1;
	}
	*thread = apic & (tpc - 1);
	while( (tpc /= 2) ){
		apic >>= 1;
	}
	*core = apic & (cpp - 1);
	while( (cpp /= 2) ){
		apic >>= 1;
	}
	*pkg = apic;
	return 0;
}

// Before this is called, pin to the desired processor (FIXME enforce?). Relies
// on the caller to free data upon error.
int x86cpuid(libtorque_cput *cpudesc,unsigned *thread,unsigned *core,
						unsigned *pkg){
	const known_x86_vendor *vendor;
	uint32_t gpregs[4];
	unsigned maxlevel;

	cpudesc->elements = 0;
	cpudesc->tlbdescs = NULL;
	cpudesc->memdescs = NULL;
	cpudesc->strdescription = NULL;
	cpudesc->tlbs = cpudesc->memories = 0;
	cpudesc->x86type = PROCESSOR_X86_UNKNOWN;
	cpudesc->threadspercore = cpudesc->coresperpackage = 0;
	cpudesc->family = cpudesc->model = cpudesc->stepping = 0;
	if(!cpuid_available()){
		return -1;
	}
	cpuid(CPUID_MAX_SUPPORT,0,gpregs);
	maxlevel = gpregs[0];
	if((vendor = lookup_vendor(gpregs + 1)) == NULL){
		return -1;
	}
	if(x86_getprocsig(maxlevel,cpudesc)){
		return -1;
	}
	if(vendor->topfxn && vendor->topfxn(maxlevel,cpudesc)){
		return -1;
	}
	if(vendor->memfxn(maxlevel,cpudesc)){
		return -1;
	}
	if(x86_getbrandname(cpudesc)){
		return -1;
	}
	if(x86topology(maxlevel,cpudesc,thread,core,pkg)){
		return -1;
	}
	return 0;
}
