#include <pthread.h>
#include <libtorque/events/thread.h>

// We're currently cancelled. That isn't generally safe unless we wrap handling
// with cancellation blocking, which eats a bit of performance. We'd like to
// encode cancellation into event handling itself FIXME.
void event_thread(void){
	int oldcstate,newcstate = PTHREAD_CANCEL_DISABLE;

	while(1){
		pthread_setcancelstate(newcstate,&oldcstate);
		pthread_setcancelstate(oldcstate,&newcstate);
		pthread_testcancel();
	}
}
