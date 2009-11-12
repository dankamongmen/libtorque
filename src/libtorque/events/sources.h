#ifndef LIBTORQUE_EVENTS_SOURCES
#define LIBTORQUE_EVENTS_SOURCES

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <libtorque/libtorque.h>

typedef libtorque_evcbfxn evcbfxn;

// The callback state associated with an event source.
typedef struct evsource {
	evcbfxn rxfxn;
	evcbfxn txfxn;
	void *cbstate;
} evsource;

struct evectors;

typedef struct evhandler {
	int efd;
	pthread_mutex_t lock;
	struct evsource *fdarray,*sigarray;
	int sigarraysize,fdarraysize;
	struct evectors *externalvec;
} evhandler;

evsource *create_evsources(unsigned)
	__attribute__ ((warn_unused_result))
	__attribute__ ((malloc));

// Central assertion: we can't generally know when a file descriptor is closed
// among the going-ons of callback processing (examples: library code with
// funky interfaces, errors, other infrastructure). Thus, setup_evsource()
// always knocks out any currently-registered handling for an fd. This is
// possible because closing an fd does remove it from event queue
// implementations for both epoll and kqueue. Since we can't base anything on
// having the fd cleared, we design to not care about it at all -- there is no
// feedback from the callback functions, and nothing needs to call anything
// upon closing an fd.
void setup_evsource(evsource *,int,evcbfxn,evcbfxn,void *);
int handle_evsource_read(evsource *,int);

int destroy_evsources(evsource *,unsigned);

#ifdef __cplusplus
}
#endif

#endif
