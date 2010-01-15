#ifndef LIBTORQUE_EVENTS_SOURCES
#define LIBTORQUE_EVENTS_SOURCES

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <pthread.h>
#include <libtorque/internal.h>
#include <libtorque/libtorque.h>

// The callback state associated with an event source.
// The alignment ought be determined at runtime based off L1 parameters, and
// combined with software indexing FIXME.

typedef struct evsourcsb {
	pthread_mutex_t lock;
	unsigned score;
} evsourcesb;

typedef struct evsource {
	evsourcesb sboard;	// source scoreboard (or use EPOLLONESHOT)
	libtorquercb rxfxn;	// read-type event callback function
	libtorquewcb txfxn;	// write-type event callback function
	libtorque_cbctx cbctx;	// libtorque internal per-source state
	void *cbstate;		// client per-source callback state
} evsource;

struct evectors;

typedef struct evthreadstats {
#define STATDEF(x) uintmax_t x;
#define PTRDEF(x) void *x;
#include <libtorque/events/x-stats.h>
#undef PTRDEF
#undef STATDEF
} evthreadstats;

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
static inline void setup_evsource(evsource *,int,libtorquercb,libtorquewcb,
					libtorque_cbctx *,void *)
	__attribute__ ((nonnull(1)));

static inline void set_evsource_rx(evsource *,int,libtorquercb)
	__attribute__ ((nonnull(1)));

static inline void set_evsource_tx(evsource *,int,libtorquewcb)
	__attribute__ ((nonnull(1)));

static inline void
set_evsource_rx(evsource *evs,int n,libtorquercb rx){
	evs[n].rxfxn = rx;
}

static inline void
set_evsource_tx(evsource *evs,int n,libtorquewcb tx){
	evs[n].txfxn = tx;
}

// We need no locking here, because the only time someone should call
// setup_evsource is when they've been handed the file descriptor from the OS,
// and not handed it off to anything else which would register it. If it was
// already being used, it must have been removed from the event queue (by
// guarantees of the epoll/kqueue mechanisms), and thus no events exist for it.
static inline void
setup_evsource(evsource *evs,int n,libtorquercb rfxn,libtorquewcb tfxn,
		libtorque_cbctx *ctx,void *v){
	set_evsource_rx(evs,n,rfxn);
	set_evsource_tx(evs,n,tfxn);
	if(ctx){
		evs[n].cbctx = *ctx;
	}else{
		memset(&evs[n].cbctx,0,sizeof(evs[n].cbctx));
	}
	evs[n].cbstate = v;
}

static inline int handle_evsource_read(evsource *,int)
	__attribute__ ((nonnull(1)));

static inline int handle_evsource_write(evsource *,int)
	__attribute__ ((nonnull(1)));

static inline int
handle_evsource_read(evsource *evs,int n){
	if(evs[n].rxfxn){
		return evs[n].rxfxn(n,&evs[n].cbctx,evs[n].cbstate);
	}
	return -1;
}

static inline int
handle_evsource_write(evsource *evs,int n){
	if(evs[n].txfxn){
		return evs[n].txfxn(n,&evs[n].cbctx,evs[n].cbstate);
	}
	return -1;
}

int destroy_evsources(evsource *);

#ifdef __cplusplus
}
#endif

#endif
