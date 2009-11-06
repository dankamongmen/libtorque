#ifndef LIBTORQUE_EVENTS_SOURCES
#define LIBTORQUE_EVENTS_SOURCES

#ifdef __cplusplus
extern "C" {
#endif

// Returning anything other than 0 will see the descriptor closed, and removed
// from the evhandler's notification queue.
// FIXME maybe ought be using a uintptr_t instead of void *?
typedef void (*evcbfxn)(unsigned,void *);

struct evsource;

struct evsource *create_evsources(unsigned)
	__attribute__ ((warn_unused_result)) __attribute__ ((malloc));

// Central assertion: we can't generally know when a file descriptor is closed
// among the going-ons of callback processing (examples: library code with
// funky interfaces, errors, other infrastructure). Thus, setup_evsource()
// always knocks out any currently-registered handling for an fd. This is
// possible because closing an fd does remove it from event queue
// implementations for both epoll and kqueue. Since we can't base anything on
// having the fd cleared, we design to not care about it at all -- there is no
// feedback from the callback functions, and nothing needs to call anything
// upon closing an fd.
void setup_evsource(struct evsource *,int,evcbfxn,evcbfxn,void *);
int handle_evsource_read(struct evsource *,unsigned);

int destroy_evsources(struct evsource *,unsigned);

#ifdef __cplusplus
}
#endif

#endif
