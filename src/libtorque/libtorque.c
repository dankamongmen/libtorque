#include <libtorque/schedule.h>
#include <libtorque/libtorque.h>
#include <libtorque/hardware/arch.h>

int libtorque_init(void){
	if(detect_architecture()){
		return -1;
	}
	if(spawn_threads()){
		free_architecture();
		return -1;
	}
	return 0;
}

int libtorque_stop(void){
	int ret = 0;

	ret |= reap_threads();
	free_architecture();
	return 0;
}
