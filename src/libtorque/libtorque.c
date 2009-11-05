#include <libtorque/libtorque.h>
#include <libtorque/hardware/arch.h>

int libtorque_init(void){
	if(detect_architecture()){
		return -1;
	}
	return 0;
}

int libtorque_stop(void){
	free_architecture();
	return 0;
}
