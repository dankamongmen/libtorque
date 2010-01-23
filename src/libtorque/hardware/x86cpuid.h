#ifndef LIBTORQUE_HARDWARE_X86CPUID
#define LIBTORQUE_HARDWARE_X86CPUID

#ifdef __cplusplus
extern "C" {
#endif

struct libtorque_cput;

int x86cpuid(struct libtorque_cput *,unsigned *,unsigned *,unsigned *);

#ifdef __cplusplus
}
#endif

#endif
