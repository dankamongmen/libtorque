#ifndef LIBTORQUE_EVENTS_EVQ
#define LIBTORQUE_EVENTS_EVQ

#ifdef __cplusplus
extern "C" {
#endif

struct evqueue;
struct torque_ctx;

int init_evqueue(struct torque_ctx *,struct evqueue *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

int destroy_evqueue(struct evqueue *)
	__attribute__ ((nonnull(1)));

#ifdef __cplusplus
}
#endif

#endif
