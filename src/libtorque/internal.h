#ifndef LIBTORQUE_INTERNAL
#define LIBTORQUE_INTERNAL

// Typedefs internal to libtorque. This file should not be installed, nor
// #include'd by applications (save those built as part of the libtorque
// distribution).

#include <stdint.h>
#include <libtorque/schedule.h>

typedef struct libtorque_topt {
	cpu_set_t schedulable;
	unsigned groupid;		// x86: Core for multicores, or package
	unsigned memories,tlbs;
	struct libtorque_memt *memdescs;
	struct libtorque_tlbt *tlbdescs;
	struct libtorque_topt *next,*sub;
} libtorque_topt;

// A node is defined as an area where all memory has the same speed as seen
// from some arbitrary set of CPUs (ignoring caches).
typedef struct libtorque_nodet {
	size_t psize;
	uintmax_t size;
	unsigned count;
	unsigned nodeid;
} libtorque_nodet;

// Whenever a field is added to this structure, make sure it's
//  a) initialized in create_libtorque_ctx(), and
//  b) free()d (and reset) in the appropriate cleanup
typedef struct libtorque_ctx {
	unsigned nodecount;		// number of NUMA nodes
	libtorque_nodet *manodes;	// NUMA node descriptors
	libtorque_topt *sched_zone;
	cpu_set_t validmap;		// affinityid_map validity map
	struct {
		unsigned thread;	// FIXME hw-genericize via dynarray
		unsigned core;
		unsigned package;
	} cpu_map[CPU_SETSIZE];
} libtorque_ctx;

#endif
