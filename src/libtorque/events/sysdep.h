#ifndef LIBTORQUE_EVENTS_SYSDEP
#define LIBTORQUE_EVENTS_SYSDEP

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>

// To emulate FreeBSD's kevent interface, supply a marshalling of two vectors.
// One's the epoll_event vector we feed directly to epoll_wait(), the other the
// data necessary to iterate over epoll_ctl() upon entry. evchanges and events
// may (but needn't) alias.
//
// See kevent(7) on FreeBSD.
#ifdef LIBTORQUE_LINUX
#include <sys/epoll.h>

#define PTR_TO_EVENTV(ev) (&(ev)->eventv)
#define PTR_TO_CHANGEV(ev) (&(ev)->changev)
typedef struct epoll_event kevententry;
#define KEVENTENTRY_FD(kptr) ((k)->data.fd)

struct kevent { // each element an array, each array the same number of members
	struct epoll_ctl_data {
		int op;
	} *ctldata; // array of ctldata
	kevententry *events; // array of kevententries
};

// Emulation of FreeBSD's kevent(2) notification mechanism. Timeout values are
// specified differently between the two, so we just disable them entirely
// (since we're never using them anyway). If they need be restored, just
// pass (t ? (t->tv_sec * 1000 + t->tv_nsec / 1000000) : -1) to epoll_wait().
static inline int
Kevent(int epfd,struct kevent *changelist,int nchanges,
		struct kevent *eventlist,int nevents){
	int ret = 0,n;

	for(n = 0 ; n < nchanges ; ++n){
		if(epoll_ctl(epfd,changelist->ctldata[n].op,
				changelist->events[n].data.fd,
				&changelist->events[n]) < 0){
			// FIXME let's get things precisely defined...divine
			// and emulate a precise definition of bsd's kevent()
			ret = -1;
		}
	}
	if(ret){
		return ret;
	}
	if(nevents == 0){
		return 0;
	}
	while((ret = epoll_wait(epfd,eventlist->events,nevents,-1)) < 0){
		if(errno != EINTR){ // loop on EINTR
			break;
		}
	}
	return ret;
}
#elif defined(LIBTORQUE_FREEBSD)
#include <stdint.h>
#include <sys/types.h>
#include <sys/event.h>

#define PTR_TO_EVENTV(ev) ((ev)->eventv)
#define PTR_TO_CHANGEV(ev) ((ev)->changev)
typedef struct kevent kevententry;
#define KEVENTENTRY_FD(kptr) ((int)(k)->ident)
#define KEVENTENTRY_SIG(kptr) ((int)(k)->ident) // Doesn't exist on Linux

#include <pthread.h>

// A pthread_testcancel() has been added at the entry, since epoll() is a
// cancellation point on Linux. Perhaps better to disable cancellation across
// both, as we have timeouts?
static inline int
Kevent(int kq,struct kevent *changelist,int nchanges,
		struct kevent *eventlist,int nevents){
	int ret;

	pthread_testcancel();
	while((ret = kevent(kq,changelist,nchanges,eventlist,nevents,NULL)) < 0){
		if(errno != EINTR){ // loop on EINTR
			break;
		}
	}
	return ret;
}
#endif

// State necessary for changing the domain of events and/or having them
// reported. One is required to do any event handling, under any scheme, and
// many plausible schemes will employ multiple evectorss.
typedef struct evectors {
	// compat-<OS>.h provides a kqueue-like interface (in terms of change
	// vectorization, which linux doesn't support) for non-BSD's
#ifdef LIBTORQUE_LINUX
	struct kevent eventv,changev;
#elif defined(LIBTORQUE_FREEBSD)
	struct kevent *eventv,*changev;
#else
#error "No operating system support for event notification"
#endif
	int vsizes,changesqueued;
} evectors;

int add_evector_kevents(struct evectors *,const struct kevent *,int);

#ifdef __cplusplus
}
#endif

#endif
