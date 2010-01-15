#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/resource.h>
#include <libtorque/internal.h>
#include <libtorque/hardware/numa.h>
#include <libtorque/hardware/arch.h>
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

static inline int
add_pagesize(unsigned *psizes,size_t **psizevals,size_t psize){
	size_t s = (*psizes + 1) * sizeof(**psizevals);
	typeof(**psizevals) *tmp;

	if((tmp = realloc(*psizevals,s)) == NULL){
		return -1;
	}
	*psizevals = tmp;
	while((unsigned)(tmp - *psizevals) < *psizes){
		if(*tmp > psize){
			memmove(tmp + 1,tmp,sizeof(*tmp) *
				(*psizes - (unsigned)(tmp - *psizevals)));
			break;
		}
		++tmp;
	}
	*tmp = psize;
	(*psizes)++;
	return 0;
}

static inline int
determine_pagesizes(const libtorque_ctx *ctx,libtorque_nodet *mem){
	unsigned cput;
	long psize;

	if((psize = sysconf(_SC_PAGESIZE)) <= 0){
		return -1;
	}
	if(add_pagesize(&mem->psizes,&mem->psizevals,psize)){
		return -1;
	}
	for(cput = 0 ; cput < ctx->cpu_typecount ; ++cput){
		const libtorque_cput *cpu;

		if((cpu = libtorque_cpu_getdesc(ctx,cput)) == NULL){
			return -1;
		}
	}
	return 0;
}

int detect_memories(libtorque_ctx *ctx){
	libtorque_nodet umamem;

	umamem.count = 1;
	umamem.psizes = 0;
	umamem.psizevals = NULL;
	if(determine_pagesizes(ctx,&umamem)){
		goto err;
	}
	if((umamem.size = determine_sysmem()) <= 0){
		goto err;
	}
	umamem.nodeid = 0;
	if(add_node(&ctx->nodecount,&ctx->manodes,&umamem) == NULL){
		goto err;
	}
	if(detect_numa(ctx)){
		goto err;
	}
	return 0;

err:
	free_memories(ctx);
	free(umamem.psizevals);
	return -1;
}

void free_memories(libtorque_ctx *ctx){
	unsigned z;

	for(z = 0 ; z < ctx->nodecount ; ++z){
		libtorque_nodet *node;

		node = &ctx->manodes[z];
		free(ctx->manodes[z].psizevals);
	}
	free(ctx->manodes);
	ctx->manodes = NULL;
	ctx->nodecount = 0;
	free_numa(ctx);
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
