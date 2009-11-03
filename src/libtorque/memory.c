#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/resource.h>
#include <libtorque/memory.h>

static unsigned nodecount;
static libtorque_nodet *manodes;

static libtorque_nodet *
add_node(unsigned *count,libtorque_nodet **nodes,const libtorque_nodet *node){
	size_t s = (*count + 1) * sizeof(**nodes);
	typeof(**nodes) *tmp;

	if((tmp = realloc(*nodes,s)) == NULL){
		return NULL;
	}
	*nodes = tmp;
	(*nodes)[*count] = *node;
	return (*nodes) + (*count)++;
}

static uintmax_t
determine_sysmem(size_t psize){
	struct rlimit rlim;
	uintmax_t ret;
	long pages;

	if((pages = sysconf(_SC_PHYS_PAGES)) <= 0){
		return 0;
	}
	ret = (uintmax_t)pages * psize;
        if(getrlimit(RLIMIT_AS,&rlim) == 0){
                if(rlim.rlim_cur > 0 && rlim.rlim_cur != RLIM_INFINITY){
			if(rlim.rlim_cur < ret){
                        	return rlim.rlim_cur;
			}
                }
        }
	return ret;
}

// FIXME there can be multiple page sizes!
static size_t
determine_pagesize(void){
	long psize;

	if((psize = sysconf(_SC_PAGESIZE)) <= 0){
		return 0;
	}
	return (size_t)psize;
}

int detect_memories(void){
	libtorque_nodet umamem;

	if((umamem.psize = determine_pagesize()) <= 0){
		return -1;
	}
	if((umamem.size = determine_sysmem(umamem.psize)) <= 0){
		return -1;
	}
	umamem.nodeid = 0;
	umamem.count = 1;
	if(add_node(&nodecount,&manodes,&umamem) == NULL){
		return -1;
	}
	return 0;
}

void free_memories(void){
	free(manodes);
	manodes = NULL;
	nodecount = 0;
}

unsigned libtorque_mem_nodecount(void){
	return nodecount;
}

const libtorque_nodet *libtorque_node_getdesc(unsigned n){
	if(n < nodecount){
		return &manodes[n];
	}
	return NULL;
}
