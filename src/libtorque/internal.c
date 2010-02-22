#include <libtorque/torque.h>

// Ugh, must be kept up-to-date viz error enumerate by hand FIXME
static const char * const errstrs[TORQUE_ERR_SYSCALL] = {
	"Unknown error",			// TORQUE_ERR_NONE
	"Assertion failure",			// TORQUE_ERR_ASSERT
	"Processor detection failure",		// TORQUE_ERR_CPUDETECT
	"Graphic processor detection failure",	// TORQUE_ERR_GPUDETECT
	"Memory detection failure",		// TORQUE_ERR_MEMDETECT
	"Affinity subsystem failure",		// TORQUE_ERR_AFFINITY
	"Insufficient system resources",	// TORQUE_ERR_RESOURCE
	"Invalid parameter",			// TORQUE_ERR_INVAL
	"Functionality unavailable on platform",// TORQUE_ERR_UNAVAIL
	// Remaining elements are looked up via subtracting TORQUE_ERR_SYSCALL
	// and calling strerror_r(3).
};

const char *torque_errstr(torque_err e){
	if(e >= sizeof(errstrs) / sizeof(*errstrs)){
		const char *str; // FIXME use strerror_r()

		if((str = strerror(e - TORQUE_ERR_SYSCALL)) == NULL){
			// FIXME this ought be internationalized
			return "Oh shit, there's a horse in the hospital!";
		}
		return str;
	}
	return errstrs[e];
}
