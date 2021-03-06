#ifndef torque_INTERNAL
#define torque_INTERNAL

// Typedefs internal to libtorque. This file should not be installed, nor
// #include'd by applications (save those built as part of the libtorque
// distribution). Anything seen by userspace must be prefixed with torque_.
// These ought only be opaque types, so the actual typedefs are free of this
// requirement (although I've usually used the same identifier).

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <libtorque/torque.h>
#include <libtorque/schedule.h>
#include <libtorque/protos/dns.h>
#include <libtorque/events/sysdep.h>

struct evsource;
struct evhandler;

// See the comment in hardware/topology.h. For simply walking the topology, the
// following rules apply:
//  - the schedulable set of cpus for a group is the union of schedulable cpus
//     of its subgroups.
//  - every group has at least one schedulable cpu.
//  - groupids are relevant only within a set of siblings.
// Leaf nodes will have cpudesc set, suitable for lookup into ctx->cpudescs
//  (note that a leaf node cannot encompass heterogeneous setups).
typedef struct torque_topt {
	cpu_set_t schedulable;
	unsigned groupid;
	unsigned cpudesc;		// only meaningful when sub == NULL
	struct torque_topt *next,*sub;
} torque_topt;

// A node is defined as an area where all memory has the same speed as seen
// from some arbitrary set of CPUs (ignoring caches).
typedef struct torque_nodet {
	uintmax_t size;			// total node size
	size_t *psizevals;		// number of page sizes
	unsigned psizes;		// architecturally-supported pagesizes
	unsigned count;			// how many of these nodes do we have
} torque_nodet;

typedef enum {
	MEMTYPE_UNKNOWN,
	MEMTYPE_CODE,
	MEMTYPE_DATA,
	MEMTYPE_UNIFIED
} torque_memtypet;

typedef struct torque_tlbt {
	unsigned entries,pagesize,associativity,sharedways;
	torque_memtypet tlbtype;
	unsigned level;
} torque_tlbt;

typedef struct torque_memt {
	uintmax_t totalsize;
	unsigned linesize,associativity,sharedways;
	torque_memtypet memtype;
	unsigned level;
} torque_memt;

typedef enum {
	PROCESSOR_X86_OEM = 0,
	PROCESSOR_X86_OVERDRIVE = 1,
	PROCESSOR_X86_DUAL = 2,
	PROCESSOR_X86_UNKNOWN
} torque_x86typet;

typedef struct x86_details {
	unsigned family,model,stepping;	// x86-specific; perhaps use a union?
					// family and model include the
					// extended family and model bits
	torque_x86typet x86type;	// ...unsure (see cpuid docs and enum)
	struct features {		// details from the feature flags
		// Introduced on the Pentium w/ MMX. Adds MMX registers.
		unsigned mmx : 1;
		// Introduced on the P3 Katmai, and supported on Athlon XP's
		// and Duron Morgans. 70 instructions. Adds XMM registers.
		unsigned sse : 1;
		// Introduced on the P4 Willamette, and supported on Opterons
		// and Athlon 64's. 144 new instructions. Extends MMX to XMM's.
		unsigned sse2 : 1;
		// SSE3 is Intel's successor to SSE2, introduced on the P4
		// Prescott. Also known as "Prescott New Instructions" (PNI).
		// 13 new instructions.
		unsigned sse3 : 1;
		// SSSE3 is Intel's successor to SSE3, introduced on Core Tejas
		// and Merom. Also known as "Tejas New Instructions" (TNI) and
		// "Merom New Instructions" (MNI). 16 new instructions.
		unsigned ssse3 : 1;
		// Introduced on Penryn Core 2. Also known as "Penryn New
		// Instructions" (PNI). 47 new instructions.
		unsigned sse41 : 1;
		// Introduced on Core i7 Nehalem. 7 new instructions.
		unsigned sse42 : 1;
		// Introduced on AMD Barcelona. 6 new instructions.
		unsigned sse4a : 1;
		// Never implemented. AMD broke SSE5 down into XOP/FMA4/CVT16
		// unsigned sse5 : 1;
		// Introduces the VEX encoding scheme, 256-bit YMM registers.
		unsigned avx : 1;
		// Main VEX/AVX support, scheduled for AMD Bulldozer.
		unsigned xop : 1;
		unsigned fma4 : 1;
		// Floating-point conversion ops, scheduled for AMD Bulldozer.
		unsigned cvt16 : 1;
	} features;
	uint32_t apic; // advanced programmable interrupt controller ID
} x86_details;

typedef struct cuda_details {
	int major,minor;		// compute capability
} cuda_details;

typedef enum { // FIXME pretty fishy...
	TORQUE_ISA_X86,
	TORQUE_ISA_NVIDIA,
} torque_isat;

typedef struct torque_cput {
	unsigned elements;		// Usable processors of this type
	unsigned memories;		// Number of memories for this type
	unsigned tlbs;			// Number of TLBs (per-mem?)
	unsigned threadspercore;	// Number of ways our core is shared
	unsigned coresperpackage;	// Number of cores sharing our die
	torque_tlbt *tlbdescs;		// TLB descriptors, NULL if tlbs == 0
	char *strdescription;		// Vender-specific string description
	torque_memt *memdescs;		// Memory descriptors
	torque_isat isa;		// instruction set architecture
	union {
		x86_details x86;	// TORQUE_ISA_X86
		cuda_details cuda;	// TORQUE_ISA_NVIDIA
	} spec;
} torque_cput;

typedef struct evtables {
	struct evsource *fdarray,*sigarray;
	unsigned sigarraysize,fdarraysize;
#ifdef TORQUE_LINUX_SIGNALFD
	int common_signalfd;
#endif
#if defined(TORQUE_LINUX) && !defined(TORQUE_LINUX_TIMERFD)
	struct itimerval itimer;
	struct evsource *timerev;
#endif
} evtables;

// evqueues are shared among some number (possibly 1) of threads. currently,
// all threads share a single evqueue, but this will almost certainly change..
typedef struct evqueue {
	int efd;			// epoll() or kqueue() file descriptor
	dns_state dnsctx;		// DNS resolution state
} evqueue;

// Whenever a field is added to this structure, make sure it's
//  a) initialized in create_torque_ctx(), and
//  b) free()d (and reset) in the appropriate cleanup
// A context describes the system state as it was when torque_init() was called,
// and as it was then restricted (ie, only those processing elements in our
// cpuset, and only those NUMA nodes which we can reach).
typedef struct torque_ctx {
	evqueue evq;			// shared evq
	unsigned nodecount;		// number of NUMA nodes
	unsigned cpu_typecount;		// number of processing element types
	torque_cput *cpudescs;		// dynarray of cpu_typecount elements
	torque_nodet *manodes;		// dynarray of NUMA node descriptors
	torque_topt *sched_zone;	// interconnection DAG (see topology.h)
	evtables eventtables;		// callback state tables
	struct evhandler *ev;		// evhandler of list leader FIXME purge
} torque_ctx;

#endif
