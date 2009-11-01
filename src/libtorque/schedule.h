#ifndef LIBTORQUE_SCHEDULE
#define LIBTORQUE_SCHEDULE

#ifdef __cplusplus
extern "C" {
#endif

// Remaining declarations are internal to libtorque via -fvisibility=hidden
int pin_thread(int);
int unpin_thread(void);

#ifdef __cplusplus
}
#endif

#endif
