#include <libtorque/arch.h>

// FIXME: we'll want to support heterogenous setups sooner rather than later
static unsigned cpu_typecount = 1;

typedef struct hwcache {
	unsigned totalsize,linesize,associativity;
} hwcache;

int detect_architecture(void){
	return 0;
}

unsigned libtorque_cpu_typecount(void){
	return cpu_typecount;
}
