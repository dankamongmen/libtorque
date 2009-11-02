#ifndef LIBTORQUE_X86CPUID
#define LIBTORQUE_X86CPUID

#ifdef __cplusplus
extern "C" {
#endif

#include <libtorque/arch.h>

// Remaining declarations are internal to libtorque via -fvisibility=hidden
int x86cpuid(libtorque_cput *);
unsigned x86apicid(void);

#ifdef __cplusplus
}
#endif

#endif
