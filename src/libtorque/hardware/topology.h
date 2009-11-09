#ifndef LIBTORQUE_HARDWARE_TOPOLOGY
#define LIBTORQUE_HARDWARE_TOPOLOGY

#ifdef __cplusplus
extern "C" {
#endif

#include <libtorque/schedule.h>

struct libtorque_cput;

// We are not considering distributed systems in this model.
//
// The scheduling universe is defined by an ordered set of $N > 1$ levels
// $L_0..L_n$. A scheduling group is a structural isomorphism, a counter $C$ of
// instances, and the $C$ affinity masks of corresponding processing elements.
// A level contains $N > 0$ scheduling groups and a description of hardware
// unique to that level. A unique surjection from classes of usable hardware to
// levels (no hardware class is described at two levels, and all usable
// hardware is described by the scheduling universe) is defined by via our
// discovery algorithm.
//
// The reality compactifies things a bit, but in theory:
// Level L_0 consists of scheduling groups over threads and processor types.
// Each successive level $L_{n+1}, n >= 0$ extends $L_n$'s elements. For each
//  element $E \in L_n$, extend $E$ through those paths reachable at equal cost
//  from all elements in $E$.
//
// Examples:
// A uniprocessor, no matter its memories, is a single topology level.
// 2 unicore, unithread SMP processors with distinct L1 and shared L2 are two
// topology levels: 2x{proc + L1} and {L2 + memory}. Split the L2, and they
// remain two topology levels: 2x{proc + L1 + L2} and {memory}. Combine both
// caches for a single level: {2proc + L1 + L2 + memory}. Sum over a level's
// processors' threads for a thread count.
//
// Note that SMT does not come into play in shaping the topology hierarchy,
// only in calculating the number of threads in a topological group. We only
// schedule to SMT processors if
//  - shared data is allocated which fits entirely within the group's cache, or
//  - we need to.
//
// The lowest level for any scheduling hierarchy is OS-schedulable entities.
// This definition is independent of "threads", "cores", "packages", etc. Our
// base composition and isomorphisms arise from schedulable entities. Should
// a single execution state ever have "multiple levels", this still works.
//
// Devices are also expressed in the topology:
//  - DMA connects to a memory
//  - DCA connects to a cache
//  - PIO connects to a processor
typedef struct libtorque_topt {
	cpu_set_t schedulable;
	unsigned groupid;		// x86: Core for multicores, or package
	struct libtorque_topt *next,*sub;
} libtorque_topt;

const libtorque_topt *libtorque_get_topology(void)
	__attribute__ ((visibility("default")));

// Remaining declarations are internal to libtorque via -fvisibility=hidden
int topologize(unsigned,unsigned,unsigned,unsigned);
void reset_topology(void);

#ifdef __cplusplus
}
#endif

#endif
