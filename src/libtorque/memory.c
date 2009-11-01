#include <stdlib.h>
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

int detect_memories(void){
	libtorque_nodet umamem;

	umamem.id = 0;
	umamem.size = 0;
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
