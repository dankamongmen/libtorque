#include <string.h>
#include <libtorque/arch.h>
#include <libtorque/schedule.h>
#include <libtorque/topology.h>

static cpu_set_t validmap;			// affinityid_map validity map
static unsigned apicid_map[CPU_SETSIZE];	// map of the local APIC table
static unsigned affinityid_map[CPU_SETSIZE];	// maps into the cpu desc table

int associate_affinityid(unsigned aid,unsigned idx,unsigned apic){
	if(aid >= sizeof(affinityid_map) / sizeof(*affinityid_map)){
		return -1;
	}
	if(CPU_ISSET(aid,&validmap)){
		return -1;
	}
	CPU_SET(aid,&validmap);
	affinityid_map[aid] = idx;
	apicid_map[aid] = apic;
	return 0;
}

void reset_topology(void){
	CPU_ZERO(&validmap);
	memset(apicid_map,0,sizeof(apicid_map));
	memset(affinityid_map,0,sizeof(affinityid_map));
}
