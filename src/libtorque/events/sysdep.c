#include <string.h>
#include <libtorque/events/sysdep.h>

// We do not enforce, but do expect and require:
//  - EV_ADD/EPOLL_CTL_ADD to be used as the control operation
//  - EPOLLET/EV_CLEAR to be used in the flags
int add_evector_kevents(evectors *e,const struct kevent *k,int kcount){
	unsigned kc;

	if(kcount < 0){
		return -1;
	}
	if(e->changesqueued + kcount >= e->vsizes){
		// FIXME be more robust. try to flush the changes to the
		// evhandler, or reallocate larger vectors. at least add stat!
		return -1;
	}
	kc = (unsigned)kcount;
#ifdef LIBTORQUE_LINUX
	memcpy(&e->changev.ctldata[e->changesqueued],k->ctldata,
			sizeof(*k->ctldata) * kc);
	memcpy(&e->changev.events[e->changesqueued],k->events,
			sizeof(*k->events) * kc);
#elif defined(LIBTORQUE_FREEBSD)
	memcpy(&e->changev[e->changesqueued],k,sizeof(*k) * kc);
#else
#error "Missing OS support"
#endif
	e->changesqueued += kcount;
	return 0;
}
