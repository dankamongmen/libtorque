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

// We must be currently pinned to the processor being associated
int associate_affinityid(unsigned aid,unsigned idx,unsigned thread,
				unsigned core,unsigned pkg){
	if(aid >= sizeof(affinityid_map) / sizeof(*affinityid_map)){
		return -1;
	}
	if(CPU_ISSET(aid,&validmap)){
		return -1;
	}
	cpu_map[aid].thread = thread;
	cpu_map[aid].core = core;
	cpu_map[aid].package = pkg;
	CPU_SET(aid,&validmap);
	affinityid_map[aid] = idx;
	return 0;
}

void reset_topology(void){
	CPU_ZERO(&validmap);
	memset(cpu_map,0,sizeof(cpu_map));
	memset(affinityid_map,0,sizeof(affinityid_map));
}

// development code
#include <stdio.h>

int print_topology(void){
   unsigned z;

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
