#ifndef LIBTORQUE_SCHEDULE
#define LIBTORQUE_SCHEDULE

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#if defined(LIBTORQUE_LINUX)
#include <sched.h>
#elif defined(LIBTORQUE_FREEBSD)
#include <sys/param.h>
#include <sys/cpuset.h>
typedef cpuset_t cpu_set_t;
#endif

struct libtorque_ctx;

// Remaining declarations are internal to libtorque via -fvisibility=hidden
int pin_thread(unsigned);
int spawn_thread(struct libtorque_ctx *);
int reap_threads(struct libtorque_ctx *);

#ifdef LIBTORQUE_FREEBSD
unsigned long pthread_self_getnumeric(void);
#elif defined(LIBTORQUE_LINUX)
static inline unsigned long
pthread_self_getnumeric(void){
	return pthread_self(); // lol
}
#endif

#ifdef __cplusplus
}
#endif

#endif
