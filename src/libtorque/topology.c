#include <string.h>
#include <libtorque/arch.h>
#include <libtorque/schedule.h>
#include <libtorque/topology.h>

static cpu_set_t validmap;			// affinityid_map validity map
static unsigned affinityid_map[CPU_SETSIZE];	// maps into the cpu desc table

int associate_affinityid(unsigned aid,unsigned idx){
	if(aid >= sizeof(affinityid_map) / sizeof(*affinityid_map)){
		return -1;
	}
	CPU_SET(aid,&validmap);
	affinityid_map[aid] = idx;
	return 0;
}

void reset_topology(void){
	CPU_ZERO(&validmap);
	memset(affinityid_map,0,sizeof(affinityid_map));
}
