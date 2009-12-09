#ifndef LIBTORQUE_EVENTS_EVQ
#define LIBTORQUE_EVENTS_EVQ

#ifdef __cplusplus
extern "C" {
#endif

struct libtorque_evq;

int initialize_evq(struct libtorque_evq *);
int destroy_evq(struct libtorque_evq *);

#ifdef __cplusplus
}
#endif

#endif
