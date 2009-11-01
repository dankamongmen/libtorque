#include <libtorque/schedule.h>

// Pins the current thread to the given cpuset ID, ie [0..cpuset_size()).
int pin_thread(int cpuid){
	if(use_cpusets == 0){
		cpu_set_t mask;

		CPU_ZERO(&mask);
		CPU_SET((unsigned)cpuid,&mask);
		if(sched_setaffinity(0,sizeof(mask),&mask)){
			return -1;
		}
		return 0;
	}
#ifdef LIBTORQUE_WITH_CPUSET
	return cpuset_pin(cpuid);
#else
	return -1;
#endif
}

// Undoes any prior pinning of this thread.
int unpin_thread(void){
	if(use_cpusets == 0){
		if(sched_setaffinity(0,sizeof(orig_cpumask),&orig_cpumask)){
			return -1;
		}
		return 0;
	}
#ifdef LIBTORQUE_WITH_CPUSET
	return cpuset_unpin();
#else
	return -1;
#endif
}

