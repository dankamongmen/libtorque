#ifndef LIBTORQUE_X86CPUID
#define LIBTORQUE_X86CPUID

#ifdef __cplusplus
extern "C" {
#endif

#include <libtorque/arch.h>

// Remaining declarations are internal to libtorque via -fvisibility=hidden
int x86cpuid(libtorque_cput *);
unsigned x86apicid(void);

// By far, the best reference here is Intel Application Note 845 (the CPUID
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

#ifdef __cplusplus
}
#endif

#endif
