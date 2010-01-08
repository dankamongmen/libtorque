#include <libtorque/libtorque.h>

// Ugh, must be kept up-to-date viz error enumerate by hand FIXME
static const char * const errstrs[LIBTORQUE_ERR_MAX + 1] = {
	"Unknown error",			// LIBTORQUE_ERR_NONE
	"Assertion failure",			// LIBTORQUE_ERR_ASSERT
	"General initialization failure",	// LIBTORQUE_ERR_INIT
	"Processor detection failure",		// LIBTORQUE_ERR_CPUDETECT
	"Memory detection failure",		// LIBTORQUE_ERR_MEMDETECT
	"Affinity subsystem failure",		// LIBTORQUE_ERR_AFFINITY

	"Invalid error identifier"		// LIBTORQUE_ERR_MAX
};

const char *libtorque_errstr(libtorque_err e){
	if(e >= sizeof(errstrs) / sizeof(*errstrs)){
		e = LIBTORQUE_ERR_MAX;
		// The error table is fucked up aiiiiieeeeeeeeee
		if(e >= sizeof(errstrs) / sizeof(*errstrs)){
			return "Oh shit, there's a horse in the hospital!";
		}
	}
	return errstrs[e];
}
