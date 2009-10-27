#ifndef LIBTORQUE_ARCH
#define LIBTORQUE_ARCH

#ifdef __cplusplus
extern "C" {
#endif

unsigned libtorque_cpu_typecount(void) __attribute__ ((visibility("default")));

int detect_architecture(void);

#ifdef __cplusplus
}
#endif

#endif
