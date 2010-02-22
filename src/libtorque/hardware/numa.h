#ifndef TORQUE_HARDWARE_NUMA
#define TORQUE_HARDWARE_NUMA

#ifdef __cplusplus
extern "C" {
#endif

struct torque_ctx;

int detect_numa(struct torque_ctx *);
void free_numa(struct torque_ctx *);

#ifdef __cplusplus
}
#endif

#endif
