#ifndef LIBTORQUE_INTERNAL
#define LIBTORQUE_INTERNAL

// Typedefs internal to libtorque. This file should not be installed, nor
// #include'd by applications (save those built as part of the libtorque
// distribution).

#include <libtorque/schedule.h>

typedef struct libtorque_topt {
	cpu_set_t schedulable;
	unsigned groupid;		// x86: Core for multicores, or package
	unsigned memories,tlbs;
	struct libtorque_memt *memdescs;
	struct libtorque_tlbt *tlbdescs;
	struct libtorque_topt *next,*sub;
} libtorque_topt;

typedef struct libtorque_ctx {
	libtorque_topt *sched_zone;
	cpu_set_t validmap;		// affinityid_map validity map
} libtorque_ctx;

#endif
