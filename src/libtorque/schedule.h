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
#include <sys/param.h>
#include <sys/cpuset.h>
typedef cpuset_t cpu_set_t;
#endif

// Remaining declarations are internal to libtorque via -fvisibility=hidden
int pin_thread(int);
int unpin_thread(void);
int detect_cpucount(void);

#ifdef __cplusplus
}
#endif

#endif
