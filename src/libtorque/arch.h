#ifndef LIBTORQUE_ARCH
#define LIBTORQUE_ARCH

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libtorque_tlbt {
	unsigned totalsize,pagesize,associativity,sharedways;
} libtorque_tlbt;

typedef struct libtorque_memt {
	unsigned totalsize,linesize,associativity,sharedways;
	enum {
		MEMTYPE_UNKNOWN,
		MEMTYPE_DATA,
		MEMTYPE_CODE,
		MEMTYPE_UNIFIED
	} memtype;
	unsigned tlbs;			// Number of TLB levels for this mem
	libtorque_tlbt *tlbdescs;	// TLB descriptors, NULL if tlbs == 0
} libtorque_memt;

typedef struct libtorque_cput {
	unsigned elements;		// Usable processors of this type
	unsigned memories;		// Number of memories for this type
	enum {				// Table 2.1, Intel IAN 845
		PROCESSOR_X86_OEM = 0,
		PROCESSOR_X86_OVERDRIVE = 1,
		PROCESSOR_X86_DUAL = 2,
		PROCESSOR_X86_UNKNOWN
	} x86type;
	unsigned family,model,stepping;	// x86-specific; perhaps use a union?
					// family and model include the
					// extended family and model bits
	char *strdescription;		// Vender-specific string description
	libtorque_memt *memdescs;	// Memory descriptors, never NULL
	// unsigned *apicids;		// FIXME not yet filled in
} libtorque_cput;

unsigned libtorque_cpu_typecount(void) __attribute__ ((visibility("default")));

const libtorque_cput *libtorque_cpu_getdesc(unsigned)
	__attribute__ ((visibility("default")));

// Remaining declarations are internal to libtorque via -fvisibility=hidden
int detect_architecture(void);
void free_architecture(void);
int pin_thread(int);
int unpin_thread(void);

#ifdef __cplusplus
}
#endif

#endif
