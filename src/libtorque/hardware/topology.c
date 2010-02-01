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
create_zone(unsigned id,unsigned cputype){
	libtorque_topt *s;

	if( (s = malloc(sizeof(*s))) ){
		CPU_ZERO(&s->schedulable);
		s->cpudesc = cputype;
		s->groupid = id;
		s->sub = NULL;
	}
	return s;
}

static libtorque_topt *
find_sched_group(libtorque_topt **sz,unsigned id,unsigned cputype){
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
	if( (n = create_zone(id,cputype)) ){
		n->next = *sz;
		*sz = n;
	}
	return n;
}

static int
share_level(libtorque_topt *sz){
	unsigned z;

	for(z = 0 ; z < CPU_SETSIZE ; ++z){
		if(CPU_ISSET(z,&sz->schedulable)){
			return 1;
		}
	}
	return 0;
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

// We must be currently pinned to the processor being associated. We need keep
// extra state during initial topology detection, but not later; this is stored
// in *tm, which must be zeroed out prior to topology detection, and not
// touched between calls. It must have space for a top_map per invocation.
torque_err topologize(libtorque_ctx *ctx,struct top_map *tm,unsigned aid,
		unsigned thread,unsigned core,unsigned pkg,unsigned cputype){
	libtorque_topt *sg;

	if(aid >= CPU_SETSIZE){
		return TORQUE_ERR_ASSERT;
	}
	if((sg = find_sched_group(&ctx->sched_zone,pkg,cputype)) == NULL){
		return TORQUE_ERR_RESOURCE;
	}
	// If we share the package, it mustn't be a new package. Quod, we
	// needn't worry about free()ing it, and can stroll on down...FIXME
	// genericize this for deeper topologies (see cpu_map[] definition)
	if(share_level(sg)){
		typeof(*sg) *sc;

		if(sg->sub == NULL){
			unsigned oid;

			if((oid = first_aid(&sg->schedulable)) >= CPU_SETSIZE){
				return TORQUE_ERR_ASSERT;
			}
			sg->sub = create_zone(tm[oid].core,cputype);
			if(sg->sub == NULL){
				return TORQUE_ERR_RESOURCE;
			}
			CPU_SET(oid,&sg->sub->schedulable);
			sg->sub->next = NULL;
		}
		if((sc = find_sched_group(&sg->sub,core,cputype)) == NULL){
			return TORQUE_ERR_RESOURCE;
		}
		CPU_SET(aid,&sc->schedulable);
	}
	CPU_SET(aid,&sg->schedulable);
	tm[aid].thread = thread;
	tm[aid].core = core;
	tm[aid].package = pkg;
	return 0;
}

static void
free_topology(libtorque_topt *top){
	while(top){
		libtorque_topt *tmp;

		free_topology(top->sub);
		tmp = top->next;
		free(top);
		top = tmp;
	}
}

void reset_topology(libtorque_ctx *ctx){
	free_topology(ctx->sched_zone);
	ctx->sched_zone = NULL;
}

static const libtorque_cput *lookup_aid_intop(const libtorque_ctx *,const libtorque_topt *,unsigned)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,2)));

static const libtorque_cput *
lookup_aid_intop(const libtorque_ctx *ctx,const libtorque_topt *top,unsigned aid){
	do{
		if(CPU_ISSET(aid,&top->schedulable)){
			if(top->sub){
				return lookup_aid_intop(ctx,top->sub,aid);
			}
			return libtorque_cpu_getdesc(ctx,top->cpudesc);
		}
	}while( (top = top->next) );
	return NULL;
}

const libtorque_cput *lookup_aid(const libtorque_ctx *ctx,unsigned aid){
	return lookup_aid_intop(ctx,ctx->sched_zone,aid);
}
