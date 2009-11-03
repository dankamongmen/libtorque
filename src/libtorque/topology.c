#include <stdlib.h>
#include <string.h>
#include <libtorque/arch.h>
#include <libtorque/schedule.h>
#include <libtorque/x86cpuid.h>
#include <libtorque/topology.h>

static cpu_set_t validmap;			// affinityid_map validity map
static struct {
	unsigned thread,core,package;
} cpu_map[CPU_SETSIZE];
static unsigned affinityid_map[CPU_SETSIZE];	// maps into the cpu desc table

// The lowest level for any scheduling hierarchy is OS-schedulable entities.
// This definition is independent of "threads", "cores", "packages", etc. Our
// base composition and isomorphisms arise from schedulable entities. Should
// a single execution state ever have "multiple levels", this still works.
static struct sgroup {
	cpu_set_t schedulable;
	unsigned groupid;		// x86: Core for multicores, or package
	struct sgroup *next,*sub;
} *sched_zone;

// development code
#include <stdio.h>

static const char *depth_terms[] = { "Package", "Core", "Thread", NULL };

static int
print_cpuset(struct sgroup *s,unsigned depth){
	unsigned z;
	int r = 0;

	if(s){
		unsigned i,lastset;
		int ret;

		for(i = 0 ; i < depth ; ++i){
			if((ret = printf("\t")) < 0){
				return -1;
			}
			r += ret;
		}
		if((ret = printf("%s %u: ",depth_terms[depth],s->groupid)) < 0){
			return -1;
		}
		r += ret;
		lastset = CPU_SETSIZE;
		for(z = 0 ; z < CPU_SETSIZE ; ++z){
			if(CPU_ISSET(z,&s->schedulable)){
				lastset = z;
				if((ret = printf("%3u ",z)) < 0){
					return -1;
				}
				r += ret;
			}
		}
		if(lastset == CPU_SETSIZE){
			return -1;
		}
		if(s->sub == NULL){
			if((ret = printf("(Processor type %u)",
					affinityid_map[lastset] + 1)) < 0){
				return -1;
			}
			r += ret;
		}
		printf("\n");
		if((ret = print_cpuset(s->sub,depth + 1)) < 0){
			return -1;
		}
		r += ret;
		if((ret = print_cpuset(s->next,depth)) < 0){
			return -1;
		}
		r += ret;
	}
	return r;
}

int print_topology(void){
	unsigned z;

	if(print_cpuset(sched_zone,0) < 0){
		return -1;
	}
	for(z = 0 ; z < sizeof(affinityid_map) / sizeof(*affinityid_map) ; ++z){
		if(CPU_ISSET(z,&validmap)){
			const libtorque_cput *cpu;

			if((cpu = libtorque_cpu_getdesc(affinityid_map[z])) == NULL){
				return -1;
			}
			printf("Cpuset ID %u: Type %u, SMT %u Core %u Package %u\n",z,
				affinityid_map[z] + 1,cpu_map[z].thread + 1,
				cpu_map[z].core + 1,cpu_map[z].package + 1);
		}
	}
	return 0;
}

static struct sgroup *
find_sched_group(struct sgroup *sz,unsigned id,unsigned *existing){
	*existing = 0;
	while(sz){
		*existing = sz->groupid;
		if(sz->groupid == id){
			break;
		}
		sz = sz->next;
	}
	return sz;
}

static struct sgroup *
create_zone(unsigned id){
	struct sgroup *s;

	if( (s = malloc(sizeof(*s))) ){
		CPU_ZERO(&s->schedulable);
		s->groupid = id;
		s->sub = NULL;
	}
	return s;
}

static int
share_pkg(struct sgroup *sz,unsigned core){
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

// We must be currently pinned to the processor being associated
int associate_affinityid(unsigned aid,unsigned idx,unsigned thread,
				unsigned core,unsigned pkg){
	struct sgroup *sg;
	unsigned extant;

	if(aid >= sizeof(affinityid_map) / sizeof(*affinityid_map)){
		return -1;
	}
	if(CPU_ISSET(aid,&validmap)){
		return -1;
	}
	// FIXME need to handle same core ID on different packages!
	if((sg = find_sched_group(sched_zone,pkg,&extant)) == NULL){
		if((sg = create_zone(pkg)) == NULL){
			return -1;
		}
		sg->next = sched_zone;
		sched_zone = sg;
	}
	// If we don't share the package, it mustn't be a new package. Quod, we
	// needn't worry about free()ing it, and can stroll on down...
	if(share_pkg(sg,core) == 0){
		typeof(*sg) *sc;

		if((sc = find_sched_group(sg->sub,core,&extant)) == NULL){
			if((sc = create_zone(core)) == NULL){
				return -1;
			}
			if((sc->next = sg->sub) == NULL){
				if((sc->next = create_zone(extant)) == NULL){
					free(sc);
					return -1;
				}
				CPU_SET(extant,&sc->next->schedulable);
				sc->next->next = NULL;
			}
			sg->sub = sc;
		}
		CPU_SET(aid,&sc->schedulable);
	}
	CPU_SET(aid,&sg->schedulable);
	cpu_map[aid].thread = thread;
	cpu_map[aid].core = core;
	cpu_map[aid].package = pkg;
	CPU_SET(aid,&validmap);
	affinityid_map[aid] = idx;
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
	memset(affinityid_map,0,sizeof(affinityid_map));
}
