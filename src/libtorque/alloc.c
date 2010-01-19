#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
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

// The default stack under NPTL is equal to RLIMIT_STACK's rlim_cur (8M on my
// Debian machine). Coloring is used inside of NPTL as of at least eglibc 2.10.
// FIXME there's a lot to do here; this is very naive
void *get_stack(size_t *s){
	if(*s == 0){
		*s = PTHREAD_STACK_MIN > SIGSTKSZ ? PTHREAD_STACK_MIN : SIGSTKSZ;
	}else if(*s < PTHREAD_STACK_MIN || *s < SIGSTKSZ){
		return NULL;
	} // round up to (which?) pagesize FIXME
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

void dealloc(void *map,size_t s){
	munmap(map,s); // FIXME really ought check for error here. but do what?
}
