#ifndef LIBTORQUE_X86CPUID
#define LIBTORQUE_X86CPUID

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <libtorque/arch.h>

// Remaining declarations are internal to libtorque via -fvisibility=hidden
int x86cpuid(libtorque_cput *);
int x86apicid(const libtorque_cput *,uint32_t *,unsigned *,unsigned *,unsigned *);

#ifdef __cplusplus
}
#endif

#endif
