#ifndef TORQUE_HARDWARE_X86CPUID
#define TORQUE_HARDWARE_X86CPUID

#ifdef __cplusplus
extern "C" {
#endif

struct torque_cput;

int x86cpuid(struct torque_cput *,unsigned *,unsigned *,unsigned *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,2,3,4)));

#ifdef __cplusplus
}
#endif

#endif
