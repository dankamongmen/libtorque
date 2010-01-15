#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/resource.h>
#include <libtorque/internal.h>
#include <libtorque/hardware/numa.h>
#include <libtorque/hardware/memory.h>

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

// rlimit_as is expressed in terms of the system's default page size
static uintmax_t
determine_sysmem(void){
	struct rlimit rlim;
	long pages,psize;
	uintmax_t ret;

	if((pages = sysconf(_SC_PHYS_PAGES)) <= 0){
		return 0;
	}
	if((psize = sysconf(_SC_PAGESIZE)) <= 0){
		return 0;
	}
	ret = (uintmax_t)pages * psize;
        if(getrlimit(RLIMIT_AS,&rlim) == 0){
                if(rlim.rlim_cur > 0 && rlim.rlim_cur != RLIM_INFINITY){
			uintmax_t asrlim = (uintmax_t)rlim.rlim_cur;

			if(asrlim < ret){
                        	return asrlim;
			}
                }
        }
	return ret;
}

// FIXME there can be multiple page sizes! extract this from tlb descriptions
static int
determine_pagesizes(libtorque_nodet *mem){
	long psize;

	if((psize = sysconf(_SC_PAGESIZE)) <= 0){
		return -1;
	}
	if((mem->psizevals = malloc(sizeof(*mem->psizevals))) == NULL){
		return -1;
	}
	mem->psizevals[0] = psize;
	mem->psizes = 1;
	return 0;
}

int detect_memories(libtorque_ctx *ctx){
	libtorque_nodet umamem;

	if(determine_pagesizes(&umamem)){
		return -1;
	}
	if((umamem.size = determine_sysmem()) <= 0){
		return -1;
	}
	umamem.nodeid = 0;
	umamem.count = 1;
	if(add_node(&ctx->nodecount,&ctx->manodes,&umamem) == NULL){
		return -1;
	}
	if(detect_numa(ctx)){
		goto err;
	}
	return 0;

err:
	free_memories(ctx);
	return -1;
}

void free_memories(libtorque_ctx *ctx){
	free_numa(ctx);
	free(ctx->manodes);
	ctx->manodes = NULL;
	ctx->nodecount = 0;
}

unsigned libtorque_mem_nodecount(const libtorque_ctx *ctx){
	return ctx->nodecount;
}

const libtorque_nodet *libtorque_node_getdesc(const libtorque_ctx *ctx,unsigned n){
	if(n < ctx->nodecount){
		return &ctx->manodes[n];
	}
	return NULL;
}
