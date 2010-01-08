#ifndef LIBTORQUE_HARDWARE_ARCH
#define LIBTORQUE_HARDWARE_ARCH

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <libtorque/libtorque.h>

struct libtorque_ctx;

unsigned libtorque_cpu_typecount(const struct libtorque_ctx *)
	__attribute__ ((visibility("default")));

const struct libtorque_cput *
libtorque_cpu_getdesc(const struct libtorque_ctx *,unsigned)
	__attribute__ ((visibility("default")));

// Remaining declarations are internal to libtorque via -fvisibility=hidden
int detect_architecture(struct libtorque_ctx *,libtorque_err *);
void free_architecture(struct libtorque_ctx *);

#ifdef __cplusplus
}
#endif

#endif
