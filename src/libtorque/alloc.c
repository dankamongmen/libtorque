#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <libtorque/alloc.h>
#include <libtorque/hardware/memory.h>

#ifdef LIBTORQUE_FREEBSD
#define MAP_ANONYMOUS MAP_ANON
#endif

void *get_pages(size_t s){
	const int flags = MAP_PRIVATE | MAP_ANONYMOUS;
	void *ret;

	if((ret = mmap(NULL,s,PROT_READ|PROT_WRITE,flags,-1,0)) == MAP_FAILED){
		ret = NULL;
	}
	return ret;
}

void *get_big_page(const struct libtorque_ctx *ctx,size_t *s){
	if((*s = large_system_pagesize(ctx)) == 0){
		return NULL;
	}
	return get_pages(*s);
}

void *mod_pages(void *map,size_t olds,size_t news){
	//const int flags = MAP_SHARED | MAP_ANONYMOUS;
	void *ret;

	if((ret = mremap(map,olds,news,MREMAP_MAYMOVE)) == MAP_FAILED){
		ret = NULL;
	}
	return ret;
}
