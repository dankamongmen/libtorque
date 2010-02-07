#ifndef TORQUE_HARDWARE_CUDA
#define TORQUE_HARDWARE_CUDA

#ifdef __cplusplus
extern "C" {
#endif

#include <libtorque/torque.h>

struct torque_cput;

torque_err cudaid(struct torque_cput *,unsigned)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

#ifdef __cplusplus
}
#endif

#endif
