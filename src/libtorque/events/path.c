#include <libtorque/events/path.h>
#include <libtorque/events/thread.h>

int add_fswatch_to_evhandler(const struct evqueue *evq __attribute__ ((unused)),
			const char *path __attribute__ ((unused)),
			libtorquercb fxn __attribute__ ((unused)),
			void *cbstate __attribute__ ((unused))){
	return -1;
}
