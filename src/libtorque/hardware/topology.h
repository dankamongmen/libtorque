#ifndef LIBTORQUE_HARDWARE_TOPOLOGY
#define LIBTORQUE_HARDWARE_TOPOLOGY

#ifdef __cplusplus
extern "C" {
#endif

#include <libtorque/libtorque.h>

// We are not considering distributed systems in this model.
//
// The scheduling universe is defined by an ordered set of $N > 1$ levels
// $L_0..L_n$. A scheduling group is a structural isomorphism, a counter $C$ of
// instances, and the $C$ affinity masks of corresponding processing elements.
// A level contains $N > 0$ scheduling groups and a description of hardware
// unique to that level. A unique surjection from classes of usable hardware to
// levels (no hardware class is described at two levels, and all usable
// hardware is described by the scheduling universe) is defined by this
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
// Note that all threads of SMT processors will be folded into a single group
// by this definition, and that heterogeneous groups are only likely at the
// topmost nodes (since heterogeneous processors likely have different caches).
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
//
// What we've described is a topology well-suited to placing handlers close
// together, ie those which share code and data. We want also, however, to
// support their separation. That'll involve the same relations, but with a
// different base isomorphism defined upon it...everywhere that we bound
// together in the first instance, we must now break apart. Or do you simply
// favor trading events across groups to within groups? Must investigate...
const struct libtorque_topt *libtorque_get_topology(struct libtorque_ctx *)
	__attribute__ ((visibility("default")))
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

struct top_map {
	unsigned thread;	// FIXME hw-genericize via dynarray
	unsigned core;
	unsigned package;
};

// Remaining declarations are internal to libtorque via -fvisibility=hidden
libtorque_err topologize(struct libtorque_ctx *,struct top_map *,unsigned,
				unsigned,unsigned,unsigned,unsigned)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,2)));

const struct libtorque_cput *lookup_aid(struct libtorque_ctx *,unsigned)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

void reset_topology(struct libtorque_ctx *);

#ifdef __cplusplus
}
#endif

#endif
