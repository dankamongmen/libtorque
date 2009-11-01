#ifndef LIBTORQUE_TOPOLOGY
#define LIBTORQUE_TOPOLOGY

#ifdef __cplusplus
extern "C" {
#endif

// A node is defined as an area where all memory has the same speed as seen
// from some arbitrary set of CPUs (ignoring caches).
typedef struct libtorque_topt {
	unsigned id,size;
} libtorque_topt;

const libtorque_topt *libtorque_sched_getdesc(int)
	__attribute__ ((visibility("default")));

// Remaining declarations are internal to libtorque via -fvisibility=hidden

#ifdef __cplusplus
}
#endif

#endif
