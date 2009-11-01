#ifndef LIBTORQUE_MEMORY
#define LIBTORQUE_MEMORY

#ifdef __cplusplus
extern "C" {
#endif

// A node is defined as an area where all memory has the same speed as seen
// from some arbitrary set of CPUs (ignoring caches).
typedef struct libtorque_nodet {
	unsigned id,size;
} libtorque_nodet;

unsigned libtorque_mem_nodecount(void) __attribute__ ((visibility("default")));

const libtorque_nodet *libtorque_node_getdesc(unsigned)
	__attribute__ ((visibility("default")));

// Remaining declarations are internal to libtorque via -fvisibility=hidden
int detect_memories(void);
void free_memories(void);

#ifdef __cplusplus
}
#endif

#endif
