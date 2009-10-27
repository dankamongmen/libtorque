#include <stdio.h>
#include <stdlib.h>
#include <libtorque/arch.h>
#include <libtorque/libtorque.h>

int main(void){
	unsigned cpu_typecount;

	if(libtorque_init()){
		fprintf(stderr,"Couldn't initialize libtorque\n");
		return EXIT_FAILURE;
	}
	if((cpu_typecount = libtorque_cpu_typecount()) <= 0){
		fprintf(stderr,"Got invalid CPU type count: %u\n",cpu_typecount);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
