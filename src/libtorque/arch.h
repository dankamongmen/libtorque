#ifndef LIBTORQUE_ARCH
#define LIBTORQUE_ARCH

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libtorque_hwcache {
	unsigned totalsize,linesize,associativity;
} libtorque_hwcache;

typedef struct libtorque_cputype {
	libtorque_hwcache *cachedescs;
	unsigned caches,elements;
} libtorque_cputype;

unsigned libtorque_cpu_typecount(void) __attribute__ ((visibility("default")));

const libtorque_cputype *libtorque_cpu_getdesc(unsigned)
	__attribute__ ((visibility("default")));

int detect_architecture(void);

#ifdef __cplusplus
}
#endif

#endif
