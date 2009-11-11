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

typedef enum {
	MEMTYPE_UNKNOWN,
	MEMTYPE_CODE,
	MEMTYPE_DATA,
	MEMTYPE_UNIFIED
} libtorque_memtypet;

typedef enum {
	PROCESSOR_X86_OEM = 0,
	PROCESSOR_X86_OVERDRIVE = 1,
	PROCESSOR_X86_DUAL = 2,
	PROCESSOR_X86_UNKNOWN
} libtorque_x86typet;

typedef struct libtorque_tlbt {
	unsigned entries,pagesize,associativity,sharedways;
	libtorque_memtypet tlbtype;
	unsigned level;
} libtorque_tlbt;

typedef struct libtorque_memt {
	uintmax_t totalsize;
	unsigned linesize,associativity,sharedways;
	libtorque_memtypet memtype;
	unsigned level;
} libtorque_memt;

typedef struct libtorque_cput {
	unsigned elements;		// Usable processors of this type
	unsigned memories;		// Number of memories for this type
	unsigned family,model,stepping;	// x86-specific; perhaps use a union?
					// family and model include the
					// extended family and model bits
	libtorque_x86typet x86type;	// stupid bullshit
	unsigned tlbs;			// Number of TLBs (per-mem?)
	unsigned threadspercore;	// Number of ways our core is shared
	unsigned coresperpackage;	// Number of cores sharing our die
	// cpu_set_t cpuset;		// Corresponding cpuset mask
	libtorque_tlbt *tlbdescs;	// TLB descriptors, NULL if tlbs == 0
	char *strdescription;		// Vender-specific string description
	libtorque_memt *memdescs;	// Memory descriptors, never NULL
} libtorque_cput;

typedef struct tdata { // FIXME opaqify!
	unsigned affinity_id;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	enum {
		THREAD_UNLAUNCHED = 0,
		THREAD_PREFAIL,
		THREAD_STARTED,
	} status;
} tdata;

// Whenever a field is added to this structure, make sure it's
//  a) initialized in create_libtorque_ctx(), and
//  b) free()d (and reset) in the appropriate cleanup
// A context describes the system state as it was when libtorque_init() was
// called, and as it was then restricted (ie, only those processing elements in
// our cpuset, and only those NUMA nodes which we can reach).
typedef struct libtorque_ctx {
	unsigned cpucount;		// number of processing elements
	unsigned nodecount;		// number of NUMA nodes
	unsigned cpu_typecount;		// number of processing element types
	libtorque_cput *cpudescs;	// dynarray of cpu_typecount elements
	libtorque_nodet *manodes;	// dynarray of NUMA node descriptors
	libtorque_topt *sched_zone;	// interconnection DAG (see topology.h)
	cpu_set_t origmask;		// these two are the same; purge one
	cpu_set_t validmap;		// affinityid validity map
	// FIXME all these CPU_SETSIZE arrays are (usually) pretty wasteful
	unsigned affinmap[CPU_SETSIZE];	// maps into the cpu desc table
	struct {
		unsigned thread;	// FIXME hw-genericize via dynarray
		unsigned core;
		unsigned package;
	} cpu_map[CPU_SETSIZE];	
	// Allocate these upon detecting cpu count, and opaqify tdata FIXME
	tdata tiddata[CPU_SETSIZE];
	pthread_t tids[CPU_SETSIZE];
	unsigned flags;			// flags to libtorque_init()
} libtorque_ctx;

#endif
