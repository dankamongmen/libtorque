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
typedef struct evsource {
	libtorquercb rxfxn;
	libtorquewcb txfxn;
	void *cbctx,*cbstate;	// cbctx is ours, cbstate is the client's
} evsource;

struct evectors;

typedef struct evthreadstats {
#define STATDEF(x) uintmax_t x;
#include <libtorque/events/x-stats.h>
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
					void *,void *)
	__attribute__ ((nonnull(1)));

// We need no locking here, because the only time someone should call
// setup_evsource is when they've been handed the file descriptor from the OS,
// and not handed it off to anything else which would register it. If it was
// already being used, it must have been removed from the event queue (by
// guarantees of the epoll/kqueue mechanisms), and thus no events exist for it.
static inline void
setup_evsource(evsource *evs,int n,libtorquercb rfxn,libtorquewcb tfxn,
		void *ctx,void *v){
	evs[n].rxfxn = rfxn;
	evs[n].txfxn = tfxn;
	evs[n].cbctx = ctx;
	evs[n].cbstate = v;
}

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

static inline int handle_evsource_read(libtorque_ctx *,evsource *,int)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,2)));

static inline int handle_evsource_write(evsource *,int)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

static inline int
handle_evsource_read(libtorque_ctx *ctx,evsource *evs,int n){
	libtorque_cbctx torquectx = {
		.ctx = ctx,
		.cbstate = evs[n].cbctx,
	};
	torquercbstate rcb = {
		.torquectx = &torquectx,
		.cbstate = evs[n].cbstate,
	};

	if(evs[n].rxfxn){
		return evs[n].rxfxn(n,&rcb);
	}
	return -1;
}

static inline int
handle_evsource_write(evsource *evs,int n){
	if(evs[n].rxfxn){
		return evs[n].txfxn(n,evs[n].cbstate);
	}
	return -1;
}

int destroy_evsources(evsource *);

#ifdef __cplusplus
}
#endif

#endif
