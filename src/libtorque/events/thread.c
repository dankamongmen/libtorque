#include <pthread.h>
#include <libtorque/events/sysdep.h>
#include <libtorque/events/thread.h>

// We're currently cancellable. That isn't generally safe unless we wrap
// handling with cancellation blocking, which eats a bit of performance. We'd
// like to encode cancellation into event handling itself FIXME.
void event_thread(void){
	int newcstate = PTHREAD_CANCEL_DISABLE;

	while(1){
		int events,oldcstate;
		evectors ev = {
			.vsizes = 0,
			.changesqueued = 0,
		}; // FIXME
		int efd = -1; // FIXME

		events = Kevent(efd,PTR_TO_CHANGEV(&ev),ev.changesqueued,
				PTR_TO_EVENTV(&ev),ev.vsizes);
		pthread_setcancelstate(newcstate,&oldcstate);
		pthread_setcancelstate(oldcstate,&newcstate);
		pthread_testcancel();
	}
}
