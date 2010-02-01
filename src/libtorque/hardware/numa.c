#include <libtorque/hardware/numa.h>

#ifndef LIBTORQUE_WITHOUT_NUMA
#include <numa.h>
#include <stdio.h>
// LibNUMA looks like the only real candidate for NUMA discovery (linux only)
int detect_numa(struct torque_ctx *ctx __attribute__ ((unused))){
	if(numa_available()){
		return 0;
	}
	// FIXME handle numa
	return 0;
}

void free_numa(struct torque_ctx *ctx __attribute__ ((unused))){
}
#else
int detect_numa(struct torque_ctx *ctx __attribute__ ((unused))){
	return 0;
}

void free_numa(struct torque_ctx *ctx __attribute__ ((unused))){
}
#endif
