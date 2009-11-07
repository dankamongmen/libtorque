#include <stdlib.h>
#include <string.h>
#include <libtorque/schedule.h>
#include <libtorque/hardware/arch.h>
#include <libtorque/hardware/x86cpuid.h>
#include <libtorque/hardware/topology.h>

static cpu_set_t validmap;			// affinityid_map validity map
static struct {
	unsigned thread,core,package;
} cpu_map[CPU_SETSIZE];

static libtorque_topt *sched_zone;

libtorque_topt *libtorque_get_topology(void){
	return sched_zone;
}

static libtorque_topt *
find_sched_group(libtorque_topt *sz,unsigned id){
	while(sz){
		if(sz->groupid == id){
			break;
		}
		sz = sz->next;
	}
	return sz;
}

static libtorque_topt *
create_zone(unsigned id){
	libtorque_topt *s;

	if( (s = malloc(sizeof(*s))) ){
		CPU_ZERO(&s->schedulable);
		s->groupid = id;
		s->sub = NULL;
	}
	return s;
}

static int
share_pkg(libtorque_topt *sz,unsigned core){
	unsigned z;

	for(z = 0 ; z < CPU_SETSIZE ; ++z){
		if(CPU_ISSET(z,&sz->schedulable)){
			if(core != cpu_map[z].core){
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
int topologize(unsigned aid,unsigned thread,unsigned core,unsigned pkg){
	libtorque_topt *sg;

	if(aid >= CPU_SETSIZE){
		return -1;
	}
	if(CPU_ISSET(aid,&validmap)){
		return -1;
	}
	if((sg = find_sched_group(sched_zone,pkg)) == NULL){
		if((sg = create_zone(pkg)) == NULL){
			return -1;
		}
		sg->next = sched_zone;
		sched_zone = sg;
	}
	// If we share the package, it mustn't be a new package. Quod, we
	// needn't worry about free()ing it, and can stroll on down...
	if(share_pkg(sg,core) == 0){
		typeof(*sg) *sc;

		if((sc = find_sched_group(sg->sub,core)) == NULL){
			if(sg->sub == NULL){
				unsigned oid;

				if((oid = first_aid(&sg->schedulable)) >= CPU_SETSIZE){
					return -1;
				}
				if((sg->sub = create_zone(cpu_map[oid].core)) == NULL){
					return -1;
				}
				CPU_SET(oid,&sg->sub->schedulable);
				sg->sub->next = NULL;
			}
			if((sc = create_zone(core)) == NULL){
				return -1;
			}
			sc->next = sg->sub;
			sg->sub = sc;
		}
		CPU_SET(aid,&sc->schedulable);
	}
	CPU_SET(aid,&sg->schedulable);
	cpu_map[aid].thread = thread;
	cpu_map[aid].core = core;
	cpu_map[aid].package = pkg;
	CPU_SET(aid,&validmap);
	return 0;
}

void reset_topology(void){
	typeof(*sched_zone) *sz;

	while( (sz = sched_zone) ){
		sched_zone = sz->next;
		free(sz);
	}
	CPU_ZERO(&validmap);
	memset(cpu_map,0,sizeof(cpu_map));
}
