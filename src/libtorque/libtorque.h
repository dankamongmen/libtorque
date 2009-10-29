#ifndef LIBTORQUE_LIBTORQUE
#define LIBTORQUE_LIBTORQUE

#ifdef __cplusplus
extern "C" {
#endif

int libtorque_init(void) __attribute__ ((visibility("default")));
int libtorque_stop(void) __attribute__ ((visibility("default")));

#ifdef __cplusplus
}
#endif

#endif
