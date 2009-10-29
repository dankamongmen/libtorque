#ifndef LIBTORQUE_ARCH
#define LIBTORQUE_ARCH

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libtorque_hwmem {
	unsigned totalsize,linesize,associativity;
} libtorque_hwmem;

typedef struct libtorque_cputype {
	libtorque_hwmem *memdescs;
	unsigned memories,elements;
} libtorque_cputype;

unsigned libtorque_cpu_typecount(void) __attribute__ ((visibility("default")));

const libtorque_cputype *libtorque_cpu_getdesc(unsigned)
	__attribute__ ((visibility("default")));

// Remaining declarations are internal to libtorque via -fvisibility=hidden
int detect_architecture(void);
void free_architecture(void);

#ifdef __cplusplus
}
#endif

#endif
