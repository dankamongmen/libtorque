#include <stdlib.h>
#include <string.h>
#include <libtorque/internal.h>
#include <libtorque/hardware/arch.h>
#include <libtorque/hardware/x86cpuid.h>
#include <libtorque/hardware/topology.h>

const libtorque_topt *libtorque_get_topology(libtorque_ctx *ctx){
	return ctx->sched_zone;
}

static inline libtorque_topt *
create_zone(unsigned id){
	libtorque_topt *s;

	if( (s = malloc(sizeof(*s))) ){
		CPU_ZERO(&s->schedulable);
		s->memories = s->tlbs = 0;
		s->memdescs = NULL;
		s->tlbdescs = NULL;
		s->groupid = id;
		s->sub = NULL;
	}
	return s;
}

static libtorque_topt *
find_sched_group(libtorque_topt **sz,unsigned id){
	libtorque_topt *n;

	while(*sz){
		if((*sz)->groupid == id){
			return (*sz);
		}
		if((*sz)->groupid > id){
			break;
		}
		sz = &(*sz)->next;
	}
	if( (n = create_zone(id)) ){
		n->next = *sz;
		*sz = n;
	}
	return n;
}

static int
share_pkg(libtorque_ctx *ctx,libtorque_topt *sz,unsigned core){
	unsigned z;

	for(z = 0 ; z < CPU_SETSIZE ; ++z){
		if(CPU_ISSET(z,&sz->schedulable)){
			if(core != ctx->cpu_map[z].core){
				return 0;
			}
		}
	}
	return 1;
}

static unsigned
first_aid(cpu_set_t *cs){
	unsigned z;

	for(z = 0 ; z < CPU_SETSIZE ; ++z){
		if(CPU_ISSET(z,cs)){
			break;
		}
	}
	return z;
}

// We must be currently pinned to the processor being associated
int topologize(libtorque_ctx *ctx,unsigned aid,unsigned thread,unsigned core,
					unsigned pkg){
	libtorque_topt *sg;

	if(aid >= CPU_SETSIZE){
		return -1;
	}
	if(CPU_ISSET(aid,&ctx->validmap)){
		return -1;
	}
	if((sg = find_sched_group(&ctx->sched_zone,pkg)) == NULL){
		return -1;
	}
	// If we share the package, it mustn't be a new package. Quod, we
	// needn't worry about free()ing it, and can stroll on down...FIXME
	// genericize this for deeper topologies (see cpu_map[] definition)
	if(share_pkg(ctx,sg,core) == 0){
		typeof(*sg) *sc;

		if(sg->sub == NULL){
			unsigned oid;

			if((oid = first_aid(&sg->schedulable)) >= CPU_SETSIZE){
				return -1;
			}
			sg->sub = create_zone(ctx->cpu_map[oid].core);
			if(sg->sub == NULL){
				return -1;
			}
			CPU_SET(oid,&sg->sub->schedulable);
			sg->sub->next = NULL;
		}
		if((sc = find_sched_group(&sg->sub,core)) == NULL){
			return -1;
		}
		CPU_SET(aid,&sc->schedulable);
	}
	CPU_SET(aid,&sg->schedulable);
	ctx->cpu_map[aid].thread = thread;
	ctx->cpu_map[aid].core = core;
	ctx->cpu_map[aid].package = pkg;
	CPU_SET(aid,&ctx->validmap);
	return 0;
}

void reset_topology(libtorque_ctx *ctx){
	typeof(*ctx->sched_zone) *sz;

	while( (sz = ctx->sched_zone) ){
		ctx->sched_zone = sz->next;
		free(sz);
	}
	CPU_ZERO(&ctx->validmap);
	memset(ctx->cpu_map,0,sizeof(ctx->cpu_map));
}
