#include <stdio.h>
#include <locale.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
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
	if((r = fprintf(fp,"%'ju%c%s",val,*unit,suffix)) < 0){
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
memory_type(torque_memtypet mtype){
	switch(mtype){
		case MEMTYPE_DATA: return "data";
		case MEMTYPE_CODE: return "code";
		case MEMTYPE_UNIFIED: return "unified";
		default: return NULL;
	}
}

static const char *
tlb_type(torque_memtypet ttype){
	switch(ttype){
		case MEMTYPE_DATA: return "data";
		case MEMTYPE_CODE: return "code";
		case MEMTYPE_UNIFIED: return "mixed";
		default: return NULL;
	}
}

static const char *
x86_type(torque_x86typet x86type){
	switch(x86type){
		case PROCESSOR_X86_OEM: return "OEM";
		case PROCESSOR_X86_OVERDRIVE: return "OverDrive"; // FIXME: TM!
		case PROCESSOR_X86_DUAL: return "MP";
		default: return NULL;
	}
}

static int
detail_memory(const torque_memt *mem){
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
detail_tlb(const torque_tlbt *tlb){
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
detail_processing_unit(const torque_cput *pudesc){
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
		const torque_memt *mem = pudesc->memdescs + n;

		printf("\tCache %u of %u: ",n + 1,pudesc->memories);
		if(detail_memory(mem)){
			return -1;
		}
	}
	for(n = 0 ; n < pudesc->tlbs ; ++n){
		const torque_tlbt *tlb = pudesc->tlbdescs + n;

		printf("\tTLB %u of %u: ",n + 1,pudesc->tlbs);
		if(detail_tlb(tlb)){
			return -1;
		}
	}
	return 0;
}

static int
detail_processing_units(const torque_ctx *ctx,unsigned cpu_typecount){
	unsigned n;

	for(n = 0 ; n < cpu_typecount ; ++n){
		const torque_cput *pudesc;

		if((pudesc = torque_cpu_getdesc(ctx,n)) == NULL){
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
detail_memory_nodes(const torque_ctx *ctx,unsigned mem_nodecount){
	unsigned n;

	for(n = 0 ; n < mem_nodecount ; ++n){
		const torque_nodet *mdesc;
		unsigned z;

		if((mdesc = torque_node_getdesc(ctx,n)) == NULL){
			fprintf(stderr,"Couldn't look up mem node %u\n",n);
			return EXIT_FAILURE;
		}
		if(mdesc->psizes <= 0){
			fprintf(stderr,"Error: page size count of %u\n",mdesc->psizes);
			return -1;
		}
		printf("(%4ux) Memory node %u of %u:\n\t",mdesc->count,
				n + 1,mem_nodecount);
		fprintf_bunit(stdout,"B",mdesc->size);
		printf(" total in ");
		for(z = 0 ; z < mdesc->psizes ; ++z){
			fprintf_bunit(stdout,"B",mdesc->psizevals[z]);
			if(z + 1 < mdesc->psizes){
				if(z + 2 == mdesc->psizes){
					printf(" and ");
				}else{
					printf(", ");
				}
			}
		}
		printf(" pages\n");
	}
	return 0;
}

static const char *depth_terms[] = { "Package", "Core", "Thread", NULL };

static int
print_cpuset(const torque_ctx *ctx,const torque_topt *s,unsigned depth){
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
			ret = printf("(%ux processor type %u)\n",
					total,s->cpudesc + 1);
		}else{
			ret = printf("(%u threads total)\n",total);
		}
		if(ret < 0){
			return -1;
		}
		r += ret;
		if((ret = print_cpuset(ctx,s->sub,depth + 1)) < 0){
			return -1;
		}
		r += ret;
		if((ret = print_cpuset(ctx,s->next,depth)) < 0){
			return -1;
		}
		r += ret;
	}
	return r;
}

static inline int
print_topology(const torque_ctx *ctx,const torque_topt *t){
	if(print_cpuset(ctx,t,0) < 0){
		return -1;
	}
	return 0;
}

static void
print_version(void){
	fprintf(stderr,"archdetect from libtorque " TORQUE_VERSIONSTR "\n");
}

static void
usage(const char *argv0){
	fprintf(stderr,"usage: %s [ options ]\n",argv0);
	fprintf(stderr,"\t-h: print this message\n");
	fprintf(stderr,"\t--version: print version info\n");
}

static int
parse_args(int argc,char **argv){
	int lflag;
	const struct option opts[] = {
		{	 .name = "version",
			.has_arg = 0,
			.flag = &lflag,
			.val = 'v',
		},
		{	 .name = NULL, .has_arg = 0, .flag = 0, .val = 0, },
	};
	const char *argv0 = *argv;
	int c;

	while((c = getopt_long(argc,argv,"h",opts,NULL)) >= 0){
		switch(c){
		case 'h':
			usage(argv0);
			exit(EXIT_SUCCESS);
		case 0: // long option
			switch(lflag){
				case 'v':
					print_version();
					exit(EXIT_SUCCESS);
				default:
					return -1;
			}
		default:
			return -1;
		}
	}
	return 0;
}

int main(int argc,char **argv){
	unsigned cpu_typecount,mem_nodecount;
	struct torque_ctx *ctx = NULL;
	const torque_topt *t;
	int ret = EXIT_FAILURE;
	const char *a0 = *argv;
	torque_err err;

	if(setlocale(LC_ALL,"") == NULL){
		fprintf(stderr,"Couldn't set locale\n");
		goto done;
	}
	if(parse_args(argc,argv)){
		fprintf(stderr,"Error parsing arguments\n");
		usage(a0);
		goto done;
	}
	if((ctx = torque_init(&err)) == NULL){
		fprintf(stderr,"Couldn't initialize libtorque (%s)\n",
				torque_errstr(err));
		goto done;
	}
	if((t = torque_get_topology(ctx)) == NULL){
		fprintf(stderr,"Couldn't look up topology\n");
		goto done;
	}
	if(print_topology(ctx,t)){
		goto done;
	}
	if((mem_nodecount = torque_mem_nodecount(ctx)) <= 0){
		fprintf(stderr,"Got invalid memory node count: %u\n",mem_nodecount);
		goto done;
	}
	if(detail_memory_nodes(ctx,mem_nodecount)){
		goto done;
	}
	if((cpu_typecount = torque_cpu_typecount(ctx)) <= 0){
		fprintf(stderr,"Got invalid CPU type count: %u\n",cpu_typecount);
		goto done;
	}
	if(detail_processing_units(ctx,cpu_typecount)){
		goto done;
	}
	ret = EXIT_SUCCESS;

done:
	if( (err = torque_stop(ctx)) ){
		fprintf(stderr,"Couldn't destroy libtorque (%s)\n",
				torque_errstr(err));
		ret = EXIT_FAILURE;
	}
	return ret;
}
