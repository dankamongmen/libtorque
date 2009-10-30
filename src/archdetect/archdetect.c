#include <stdio.h>
#include <stdlib.h>
#include <libtorque/arch.h>
#include <libtorque/libtorque.h>

static const char *
memory_type(int mtype){
	switch(mtype){
		case MEMTYPE_DATA: return "Data";
		case MEMTYPE_CODE: return "Code";
		case MEMTYPE_UNIFIED: return "Unified";
		default: return NULL;
	}
}

static const char *
x86_type(int x86type){
	switch(x86type){
		case PROCESSOR_X86_OEM: return "OEM";
		case PROCESSOR_X86_OVERDRIVE: return "OverDrive"; // FIXME: TM!
		case PROCESSOR_X86_DUAL: return "MP";
		default: return NULL;
	}
}

static int
detail_processing_unit(const libtorque_cput *pudesc){
	const char *x86type;
	unsigned n;

	if(pudesc->strdescription == NULL){
		fprintf(stderr,"Error: no string description\n");
		return -1;
	}
	if((x86type = x86_type(pudesc->x86type)) == NULL){
		fprintf(stderr,"Error: invalid x86 type information\n");
		return -1;
	}
	printf("\tBrand name: %s (%s)\n",pudesc->strdescription,x86type);
	if(pudesc->family == 0){
		fprintf(stderr,"Error: invalid processor family\n");
		return -1;
	}
	printf("\tFamily: 0x%03x (%d)\tModel: 0x%02x (%d)\tStepping: %2d\n",
		pudesc->family,pudesc->family,pudesc->model,pudesc->model,pudesc->stepping);
	if(pudesc->memories <= 0){
		fprintf(stderr,"Error: memory count of 0\n");
		return -1;
	}
	for(n = 0 ; n < pudesc->memories ; ++n){
		const libtorque_memt *mem = pudesc->memdescs + n;
		const char *memt;

		if((memt = memory_type(mem->memtype)) == NULL){
			fprintf(stderr,"Error: invalid or unknown memory type\n");
			return -1;
		}
		if(mem->totalsize <= 0){
			fprintf(stderr,"Error: memory total size of 0\n");
			return -1;
		}
		if(mem->linesize <= 0){
			fprintf(stderr,"Error: memory linesize of 0\n");
			return -1;
		}
		if(mem->associativity <= 0){
			fprintf(stderr,"Error: memory associativity of 0\n");
			return -1;
		}
		if(mem->sharedways <= 0){
			fprintf(stderr,"Error: memory sharedways of 0\n");
			return -1;
		}
		printf("\tMemory %u of %u: %ub total, %ub line, %u-assoc, ",
			n + 1,pudesc->memories,mem->totalsize,mem->linesize,
			mem->associativity);
		if(mem->sharedways == 1){
			printf("unshared ");
		}else{
			printf("%u-shared ",mem->sharedways);
		}
		printf("(%s)\n",memt);
	}
	return 0;
}

static int
detail_processing_units(unsigned cpu_typecount){
	unsigned n;

	for(n = 0 ; n < cpu_typecount ; ++n){
		const libtorque_cput *pudesc;

		if((pudesc = libtorque_cpu_getdesc(n)) == NULL){
			fprintf(stderr,"Couldn't look up CPU type %u\n",n);
			return EXIT_FAILURE;
		}
		if(pudesc->elements <= 0){
			fprintf(stderr,"Error: element count of 0\n");
			return -1;
		}
		printf("Processing unit type %u of %u (count: %u):\n",
				n + 1,cpu_typecount,pudesc->elements);
		if(detail_processing_unit(pudesc)){
			return -1;
		}
	}
	return 0;
}

int main(void){
	unsigned cpu_typecount;
	int ret = EXIT_FAILURE;

	if(libtorque_init()){
		fprintf(stderr,"Couldn't initialize libtorque\n");
		return EXIT_FAILURE;
	}
	if((cpu_typecount = libtorque_cpu_typecount()) <= 0){
		fprintf(stderr,"Got invalid CPU type count: %u\n",cpu_typecount);
		goto done;
	}
	if(detail_processing_units(cpu_typecount)){
		goto done;
	}
	ret = EXIT_SUCCESS;

done:
	if(libtorque_stop()){
		fprintf(stderr,"Couldn't destroy libtorque\n");
		return EXIT_FAILURE;
	}
	return ret;
}
