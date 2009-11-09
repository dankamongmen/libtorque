#include <stdio.h>
#include <stdlib.h>
#include <libtorque/libtorque.h>

int main(void){
	if(libtorque_init()){
		fprintf(stderr,"Couldn't initialize libtorque\n");
		return EXIT_FAILURE;
	}
	if(libtorque_stop()){
		fprintf(stderr,"Couldn't shutdown libtorque\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
