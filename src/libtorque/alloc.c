#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <libtorque/alloc.h>
#include <libtorque/hardware/memory.h>

#ifdef TORQUE_FREEBSD
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

void *get_big_page(const struct torque_ctx *ctx,size_t *s){
	if((*s = large_system_pagesize(ctx)) == 0){
		return NULL;
	}
	return get_pages(*s);
}

// The default stack under NPTL is equal to RLIMIT_STACK's rlim_cur (8M on
// my Debian machine). Coloring is used inside of NPTL as of at least
// eglibc 2.10. PTHREAD_STACK_MIN is only 16k(!), and SIGSTKSZ 8k.
// // FIXME there's a lot to do here; this is very naive
void *get_stack(size_t *s){
	if(*s == 0){
		struct rlimit rl;

		if(getrlimit(RLIMIT_STACK,&rl)){
			return NULL;
		}
		if(rl.rlim_cur == RLIM_INFINITY){
			*s = PTHREAD_STACK_MIN > SIGSTKSZ ? PTHREAD_STACK_MIN : SIGSTKSZ;
		}else{
			*s = rl.rlim_cur * 1024; // RLIMIT_STACK is in KB
		}
	}
	if(*s < PTHREAD_STACK_MIN || *s < SIGSTKSZ){
		return NULL;
	} // round up to (which?) pagesize FIXME
	return get_pages(*s);
}

void *mod_pages(void *map,size_t olds,size_t news){
	void *ret;

#ifdef TORQUE_LINUX
	if((ret = mremap(map,olds,news,MREMAP_MAYMOVE)) == MAP_FAILED){
		ret = NULL;
	}
#else
        // From mmap(2) on freebsd 6.3: A successful FIXED mmap deletes any
        // previous mapping in the allocated address range. This means:
        // remapping over a current map will blow it away (unless FIXED isn't
        // provided, in which case it can't overlap an old mapping). If we were
	// using a file-based mapping here, we'd offset at "olds" rather than 0.
        if((ret = mmap((char *)map + olds,news - olds,PROT_READ|PROT_WRITE,
			MAP_PRIVATE|MAP_ANONYMOUS,-1,0)) == MAP_FAILED){
		ret = NULL; // We couldn't get the memory whatsoever
        }else if(ret != (char *)map + olds){  // Did we squash?
                // We got the memory, but not where we wanted it. Copy over the
                // old map, and then free it up...
                munmap(ret,news - olds);
                if((ret = mmap(NULL,news,PROT_READ|PROT_WRITE,
				MAP_PRIVATE|MAP_ANONYMOUS,-1,0)) == MAP_FAILED){
                        return ret;
                }
                memcpy(ret,map,olds);
                munmap(map,olds); // Free the old mapping
        } // We successfully squashed: ret is a pointer to the original buf.
#endif
	return ret;
}

void dealloc(void *map,size_t s){
	munmap(map,s); // FIXME really ought check for error here. but do what?
}

/* void *hwaligned_alloc_tight(size_t s,size_t *afac){
}

void *hwaligned_alloc_tight(size_t s,size_t *afac){
}*/
