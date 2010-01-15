#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <libtorque/alloc.h>

void *get_pages(size_t s){
	const int flags = MAP_SHARED | MAP_ANONYMOUS;

	return mmap(NULL,s,PROT_READ|PROT_WRITE,flags,-1,0);
}

void *get_big_page(size_t *s){
	*s = getpagesize() * 4; // FIXME
	return get_pages(*s);
}

void *mod_pages(void *map,size_t olds,size_t news){
	const int flags = MAP_SHARED | MAP_ANONYMOUS;
	void *ret;

	// FIXME this is slow and nasty. why doesn't mremap() work?
	if((ret = mmap(map,news,PROT_READ|PROT_WRITE,flags,-1,0)) == MAP_FAILED){
		ret = NULL;
	}else if(ret != map){
		memcpy(ret,map,olds);
		munmap(map,olds);
	}
	return ret;
}
