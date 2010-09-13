#ifndef LIBTORQUE_SCHEDULE
#define LIBTORQUE_SCHEDULE

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#if defined(TORQUE_LINUX)
#include <sched.h>
#elif defined(TORQUE_FREEBSD)
#include <sys/param.h>
#include <sys/cpuset.h>
typedef cpuset_t cpu_set_t;
#else
#error "Need cpu_set_t definition for this platform" // solaris is psetid_t(?)
#endif

// FreeBSD's cpuset.h (as of 7.2) doesn't provide CPU_COUNT, nor do older
// Linux setups (including RHEL5). This requires only CPU_{SETSIZE,ISSET}.
static inline unsigned
portable_cpuset_count(const cpu_set_t *mask){
	unsigned count = 0,cpu;

	for(cpu = 0 ; cpu < CPU_SETSIZE ; ++cpu){
		if(CPU_ISSET(cpu,mask)){
			++count;
		}
	}
	return count;
}

struct torque_ctx;

int pin_thread(unsigned);
int spawn_thread(struct torque_ctx *);
int reap_threads(struct torque_ctx *);
int block_threads(struct torque_ctx *);
int get_thread_aid(void);

#ifdef TORQUE_FREEBSD
unsigned long pthread_self_getnumeric(void);
#elif defined(TORQUE_LINUX)
static inline unsigned long
pthread_self_getnumeric(void){
	return pthread_self(); // lol
}
#endif

#ifdef __cplusplus
}
#endif

#endif
