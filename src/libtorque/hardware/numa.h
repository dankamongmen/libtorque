#ifndef LIBTORQUE_HARDWARE_NUMA
#define LIBTORQUE_HARDWARE_NUMA

#ifdef __cplusplus
extern "C" {
#endif

struct libtorque_ctx;

// Remaining declarations are internal to libtorque via -fvisibility=hidden
int detect_numa(struct libtorque_ctx *);
void free_numa(struct libtorque_ctx *);

#ifdef __cplusplus
}
#endif

#endif
