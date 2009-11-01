#ifndef LIBTORQUE_SCHEDULE
#define LIBTORQUE_SCHEDULE

#ifdef __cplusplus
extern "C" {
#endif

#if defined(LIBTORQUE_WITH_CPUSET)
#include <cpuset.h>
#endif
#if defined(LIBTORQUE_LINUX)
#include <sched.h>
#elif defined(LIBTORQUE_FREEBSD)
#include <sys/cpuset.h>
typedef cpusetid cpu_set_t;
#endif

// Remaining declarations are internal to libtorque via -fvisibility=hidden
int pin_thread(int);
int unpin_thread(cpu_set_t *);
int detect_cpucount(cpu_set_t *);

#ifdef __cplusplus
}
#endif

#endif
