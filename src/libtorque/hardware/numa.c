#include <libtorque/hardware/numa.h>

#ifndef LIBTORQUE_WITHOUT_NUMA
#include <numa.h>
#include <stdio.h>
// LibNUMA looks like the only real candidate for NUMA discovery (linux only)
int detect_numa(struct libtorque_ctx *ctx __attribute__ ((unused))){
	printf("check for numa\n");
	if(numa_available()){
		printf("nope\n");
		return 0;
	}
	printf("ayup\n");
	return 0;
}

void free_numa(struct libtorque_ctx *ctx __attribute__ ((unused))){
}
#else
int detect_numa(struct libtorque_ctx *ctx __attribute__ ((unused))){
	return 0;
}

void free_numa(struct libtorque_ctx *ctx __attribute__ ((unused))){
}
#endif
