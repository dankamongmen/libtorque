#ifndef torque_HARDWARE_ARCH
#define torque_HARDWARE_ARCH

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <libtorque/libtorque.h>

struct torque_ctx;

unsigned torque_cpu_typecount(const struct torque_ctx *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

const struct torque_cput *
torque_cpu_getdesc(const struct torque_ctx *,unsigned)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

// Remaining declarations are internal to libtorque via -fvisibility=hidden
torque_err detect_architecture(struct torque_ctx *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

void free_architecture(struct torque_ctx *);

#ifdef __cplusplus
}
#endif

#endif
