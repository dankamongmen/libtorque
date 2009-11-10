#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libtorque/internal.h>
#include <libtorque/libtorque.h>
#include <libtorque/hardware/arch.h>
#include <libtorque/hardware/memory.h>
#include <libtorque/hardware/topology.h>

static int
fprintf_bunit(FILE *fp,const char *suffix,uintmax_t val){
	const char units[] = "KMGTPEZY",*unit = units;
	const uintmax_t SCALE = 1024;
	int r;

	if(val < SCALE || (val % SCALE)){
		return fprintf(fp,"%ju%s",val,suffix);
	}
	val /= SCALE;
	while(val >= SCALE && (val % SCALE == 0)){
		val /= SCALE;
		if(!*++unit){ // damn, chief!
			return -1;
		}
	}
	if((r = fprintf(fp,"%ju%c%s",val,*unit,suffix)) < 0){
		return r;
	}
	if(val >= SCALE && (val % SCALE)){
		// It's weird to see something like "1009.756MB"; you wish to
		// perceive it as a G and change, but in reality it is less!
		while(*++unit && (val / SCALE >= 1000) && (val % SCALE)){
			val /= SCALE;
		}
		if(*unit){
			return fprintf(fp," (%.3f %c%s)",(float)val / SCALE,*unit,suffix);
		}
	}
	return 0;
}

static const char *
memory_type(libtorque_memtypet mtype){
	switch(mtype){
		case MEMTYPE_DATA: return "data";
		case MEMTYPE_CODE: return "code";
		case MEMTYPE_UNIFIED: return "unified";
		default: return NULL;
	}
}

static const char *
tlb_type(libtorque_memtypet ttype){
	switch(ttype){
		case MEMTYPE_DATA: return "data";
		case MEMTYPE_CODE: return "code";
		case MEMTYPE_UNIFIED: return "mixed";
		default: return NULL;
	}
}

static const char *
x86_type(libtorque_x86typet x86type){
	switch(x86type){
		case PROCESSOR_X86_OEM: return "OEM";
		case PROCESSOR_X86_OVERDRIVE: return "OverDrive"; // FIXME: TM!
		case PROCESSOR_X86_DUAL: return "MP";
		default: return NULL;
	}
}

static int
detail_memory(const libtorque_memt *mem){
	const char *memt;

	if((memt = memory_type(mem->memtype)) == NULL){
		fprintf(stderr,"Error: invalid or unknown memory type %d\n",mem->memtype);
		return -1;
	}
	if(mem->totalsize <= 0){
		fprintf(stderr,"Error: memory total size of %ju\n",mem->totalsize);
		return -1;
	}
	if(mem->linesize <= 0){
		fprintf(stderr,"Error: memory linesize of %u\n",mem->linesize);
		return -1;
	}
	if(mem->associativity <= 0){
		fprintf(stderr,"Error: memory associativity of %u\n",mem->associativity);
		return -1;
	}
	if(mem->sharedways <= 0){
		fprintf(stderr,"Error: memory sharedways of %u\n",mem->sharedways);
		return -1;
	}
	if(mem->level <= 0){
		fprintf(stderr,"Error: memory level of %u\n",mem->level);
		return -1;
	}
	if(fprintf_bunit(stdout,"B",mem->totalsize) < 0){
		return -1;
	}
	printf(" total, ");
	if(fprintf_bunit(stdout,"B",(uintmax_t)mem->linesize) < 0){
		return -1;
	}
	printf(" line, %u-assoc, ",mem->associativity);
	if(mem->sharedways == 1){
		printf("unshared ");
	}else{
		printf("%u-shared ",mem->sharedways);
	}
	printf("(L%u %s)\n",mem->level,memt);
	return 0;
}

static int
detail_tlb(const libtorque_tlbt *tlb){
	const char *tlbt;

	if((tlbt = tlb_type(tlb->tlbtype)) == NULL){
		fprintf(stderr,"Error: invalid or unknown tlbory type %d\n",tlb->tlbtype);
		return -1;
	}
	if(tlb->pagesize <= 0){
		fprintf(stderr,"Error: TLB pagesize of %u\n",tlb->pagesize);
		return -1;
	}
	if(tlb->associativity <= 0){
		fprintf(stderr,"Error: TLB associativity of %u\n",tlb->associativity);
		return -1;
	}
	if(tlb->sharedways <= 0){
		fprintf(stderr,"Error: TLB sharedways of %u\n",tlb->sharedways);
		return -1;
	}
	if(tlb->entries <= 0){
		fprintf(stderr,"Error: TLB entries of %u\n",tlb->entries);
		return -1;
	}
	if(tlb->level <= 0){
		fprintf(stderr,"Error: TLB level of %u\n",tlb->level);
		return -1;
	}
	if(fprintf_bunit(stdout,"B",(uintmax_t)tlb->pagesize) < 0){
		return -1;
	}
	printf(" pages, ");
	if(fprintf_bunit(stdout,"-entry",(uintmax_t)tlb->entries) < 0){
		return -1;
	}
	printf(", %u-assoc, ",tlb->associativity);
	if(tlb->sharedways == 1){
		printf("unshared ");
	}else{
		printf("%u-shared ",tlb->sharedways);
	}
	printf("(L%u %s)\n",tlb->level,tlbt);
	return 0;
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
	if(pudesc->threadspercore <= 0){
		fprintf(stderr,"Error: invalid SMT information\n");
		return -1;
	}
	printf("\tBrand name: %s (%s)\n",pudesc->strdescription,x86type);
	if(pudesc->family == 0){
		fprintf(stderr,"Error: invalid processor family\n");
		return -1;
	}
	printf("\tFamily: 0x%03x (%d) Model: 0x%02x (%d) Stepping: %d\n",
		pudesc->family,pudesc->family,pudesc->model,pudesc->model,pudesc->stepping);
	if(pudesc->threadspercore == 1){
		printf("\t1 thread per processing core, %u core%s per package\n",
				pudesc->coresperpackage,
				pudesc->coresperpackage != 1 ? "s" : "");
	}else{
		printf("\t%u threads per processing core, %u core%s (%u logical) per package\n",
				pudesc->threadspercore,
				pudesc->coresperpackage,
				pudesc->coresperpackage != 1 ? "s" : "",
				pudesc->coresperpackage - pudesc->coresperpackage / pudesc->threadspercore);
	}
	if(pudesc->memories <= 0){
		fprintf(stderr,"Error: memory count of 0\n");
		return -1;
	}
	for(n = 0 ; n < pudesc->memories ; ++n){
		const libtorque_memt *mem = pudesc->memdescs + n;

		printf("\tCache %u of %u: ",n + 1,pudesc->memories);
		if(detail_memory(mem)){
			return -1;
		}
	}
	for(n = 0 ; n < pudesc->tlbs ; ++n){
		const libtorque_tlbt *tlb = pudesc->tlbdescs + n;

		printf("\tTLB %u of %u: ",n + 1,pudesc->tlbs);
		if(detail_tlb(tlb)){
			return -1;
		}
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
			fprintf(stderr,"Error: element count of %u\n",pudesc->elements);
			return -1;
		}
		printf("(%4ux) Processing unit type %u of %u:\n",
				pudesc->elements,n + 1,cpu_typecount);
		if(detail_processing_unit(pudesc)){
			return -1;
		}
	}
	return 0;
}

static int
detail_memory_nodes(const libtorque_ctx *ctx,unsigned mem_nodecount){
	unsigned n;

	for(n = 0 ; n < mem_nodecount ; ++n){
		const libtorque_nodet *mdesc;

		if((mdesc = libtorque_node_getdesc(ctx,n)) == NULL){
			fprintf(stderr,"Couldn't look up mem node %u\n",n);
			return EXIT_FAILURE;
		}
		if(mdesc->psize <= 0){
			fprintf(stderr,"Error: page size of %zu\n",mdesc->psize);
			return -1;
		}
		printf("(%4ux) Memory node %u of %u:\n\t",mdesc->count,
				n + 1,mem_nodecount);
		fprintf_bunit(stdout,"B",mdesc->size);
		printf(" total, ");
		fprintf_bunit(stdout,"B",mdesc->psize);
		printf(" pages\n");
	}
	return 0;
}

static const char *depth_terms[] = { "Package", "Core", "Thread", NULL };

static int
print_cpuset(const libtorque_topt *s,unsigned depth){
	unsigned z;
	int r = 0;

	if(s){
		unsigned i,lastset,total;
		int ret;

		i = 0;
		do{
			if((ret = printf("\t")) < 0){
				return -1;
			}
			r += ret;
		}while(i++ < depth);
		if((ret = printf("%s %u: ",depth_terms[depth],s->groupid)) < 0){
			return -1;
		}
		r += ret;
		lastset = CPU_SETSIZE;
		total = 0;
		for(z = 0 ; z < CPU_SETSIZE ; ++z){
			if(CPU_ISSET(z,&s->schedulable)){
				++total;
				lastset = z;
				if(s->sub == NULL){
					if((ret = printf("%3u ",z)) < 0){
						return -1;
					}
					r += ret;
				}
			}
		}
		if(total == 0 || lastset == CPU_SETSIZE){
			return -1;
		}
		if(s->sub == NULL){
			ret = printf("(%ux processor type %u)\n",total,
				libtorque_affinitymapping(lastset) + 1);
		}else{
			ret = printf("(%u threads total)\n",total);
		}
		if(ret < 0){
			return -1;
		}
		r += ret;
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

static inline int
print_topology(const libtorque_topt *t){
	if(print_cpuset(t,0) < 0){
		return -1;
	}
	return 0;
}

int main(void){
	unsigned cpu_typecount,mem_nodecount;
	struct libtorque_ctx *ctx = NULL;
	const libtorque_topt *t;
	int ret = EXIT_FAILURE;

	if((ctx = libtorque_init()) == NULL){
		fprintf(stderr,"Couldn't initialize libtorque\n");
		goto done;
	}
	if((t = libtorque_get_topology(ctx)) == NULL){
		fprintf(stderr,"Couldn't look up topology\n");
		goto done;
	}
	if(print_topology(t)){
		goto done;
	}
	if((mem_nodecount = libtorque_mem_nodecount(ctx)) <= 0){
		fprintf(stderr,"Got invalid memory node count: %u\n",mem_nodecount);
		goto done;
	}
	if(detail_memory_nodes(ctx,mem_nodecount)){
		goto done;
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
	if(libtorque_stop(ctx)){
		fprintf(stderr,"Couldn't destroy libtorque\n");
		ret = EXIT_FAILURE;
	}
	return ret;
}
