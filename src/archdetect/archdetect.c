#include <stdlib.h>
#include <libtorque/libtorque.h>

int main(void){
	if(libtorque_init()){
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
