#include <libtorque/arch.h>
#include <libtorque/libtorque.h>

int libtorque_init(void){
	if(detect_architecture()){
		return -1;
	}
	return 0;
}
