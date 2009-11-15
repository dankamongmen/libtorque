#ifndef LIBTORQUE_EVENTS_SOURCES
#define LIBTORQUE_EVENTS_SOURCES

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <pthread.h>
#include <libtorque/libtorque.h>

typedef struct evsource { // The callback state associated with an event source
	libtorquecb rxfxn,txfxn;
	void *cbstate;
} evsource;

struct evectors;

typedef struct evthreadstats {
	uintmax_t evhandler_errors;
} evthreadstats;

typedef struct evhandler {
	int efd;
	evthreadstats stats;
	pthread_mutex_t lock;
	struct evsource *fdarray,*sigarray;
	unsigned sigarraysize,fdarraysize;
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
void setup_evsource(evsource *,int,libtorquecb,libtorquecb,void *)
	__attribute__ ((nonnull(1)));

int handle_evsource_read(evsource *,int)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

int handle_evsource_write(evsource *,int)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

int destroy_evsources(evsource *,unsigned);

#ifdef __cplusplus
}
#endif

#endif
