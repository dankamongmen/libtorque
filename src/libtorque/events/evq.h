#ifndef LIBTORQUE_EVENTS_EVQ
#define LIBTORQUE_EVENTS_EVQ

#ifdef __cplusplus
extern "C" {
#endif

struct evqueue;

int init_evqueue(struct evqueue *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

int destroy_evqueue(struct evqueue *);

#ifdef __cplusplus
}
#endif

#endif
