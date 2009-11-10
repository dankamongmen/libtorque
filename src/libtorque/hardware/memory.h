#ifndef LIBTORQUE_HARDWARE_MEMORY
#define LIBTORQUE_HARDWARE_MEMORY

#ifdef __cplusplus
extern "C" {
#endif

struct libtorque_ctx;
struct libtorque_nodet;

unsigned libtorque_mem_nodecount(const struct libtorque_ctx *)
	__attribute__ ((visibility("default")));

const struct libtorque_nodet *
libtorque_node_getdesc(const struct libtorque_ctx *,unsigned)
	__attribute__ ((visibility("default")));

// Remaining declarations are internal to libtorque via -fvisibility=hidden
int detect_memories(struct libtorque_ctx *);
void free_memories(struct libtorque_ctx *);

#ifdef __cplusplus
}
#endif

#endif
