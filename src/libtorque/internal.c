#include <libtorque/torque.h>

// Ugh, must be kept up-to-date viz error enumerate by hand FIXME
static const char * const errstrs[TORQUE_ERR_MAX + 1] = {
	"Unknown error",			// TORQUE_ERR_NONE
	"Assertion failure",			// TORQUE_ERR_ASSERT
	"Processor detection failure",		// TORQUE_ERR_CPUDETECT
	"Memory detection failure",		// TORQUE_ERR_MEMDETECT
	"Affinity subsystem failure",		// TORQUE_ERR_AFFINITY
	"Insufficient system resources",	// TORQUE_ERR_RESOURCE
	"Invalid parameter",			// TORQUE_ERR_INVAL
	"Functionality unavailable on platform",// TORQUE_ERR_UNAVAIL

	"Invalid error identifier"		// TORQUE_ERR_MAX
};

const char *torque_errstr(torque_err e){
	if(e >= sizeof(errstrs) / sizeof(*errstrs)){
		e = TORQUE_ERR_MAX;
		// The error table is fucked up aiiiiieeeeeeeeee
		if(e >= sizeof(errstrs) / sizeof(*errstrs)){
			return "Oh shit, there's a horse in the hospital!";
		}
	}
	return errstrs[e];
}
