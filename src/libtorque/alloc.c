#include <stdio.h>
#include <sys/mman.h>
#include <libtorque/alloc.h>

void *get_pages(size_t s,int prot){
	const int flags = MAP_SHARED | MAP_ANONYMOUS;

	return mmap(NULL,s,prot,flags,-1,0);
}

void *mod_pages(void *map,size_t olds,size_t news,int prot){
	return mremap(map,olds,news,prot);
}
