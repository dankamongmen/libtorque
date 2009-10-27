#include <libtorque/arch.h>

typedef struct hwcache {
	unsigned totalsize,linesize,associativity;
} hwcache;

int detect_architecture(void){
	return 0;
}
