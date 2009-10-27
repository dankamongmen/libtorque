#include <stdio.h>
#include <stdlib.h>
#include <libtorque/arch.h>
#include <libtorque/libtorque.h>

static int
detail_processing_unit(const libtorque_cputype *pudesc){
	if(pudesc->elements <= 0){
		fprintf(stderr,"Error: element count of 0\n");
		return -1;
	}
	printf("\t\tCount: %u\n",pudesc->elements);
	printf("\t\tCaches: %u\n",pudesc->caches);
	return 0;
}

static int
detail_processing_units(unsigned cpu_typecount){
	unsigned n;

	for(n = 0 ; n < cpu_typecount ; ++n){
		const libtorque_cputype *pudesc;

		if((pudesc = libtorque_cpu_getdesc(n)) == NULL){
			fprintf(stderr,"Couldn't look up CPU type %u\n",n);
			return EXIT_FAILURE;
		}
		printf("\tProcessing unit type %u:\n",n + 1);
		if(detail_processing_unit(pudesc)){
			return -1;
		}
	}
	return 0;
}

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
	printf("Detected %u processing unit type%s\n",
			cpu_typecount,cpu_typecount == 1 ? "" : "s");
	if(detail_processing_units(cpu_typecount)){
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
