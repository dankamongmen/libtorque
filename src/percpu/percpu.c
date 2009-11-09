#include <stdio.h>
#include <stdlib.h>
#include <libtorque/libtorque.h>

static void
usage(const char *argv0){
	fprintf(stderr,"usage: %s command [ args... ]\n",argv0);
}

int main(int argc,char **argv){
	if(argc == 1){
		usage(*argv);
		return EXIT_FAILURE;
	}
	if(libtorque_init()){
		fprintf(stderr,"Error initializing libtorque.\n");
		return EXIT_FAILURE;
	}
	if(libtorque_spawn()){
		fprintf(stderr,"Error spawning libtorque.\n");
		goto err;
	}
	if(libtorque_stop()){
		fprintf(stderr,"Error shutting down libtorque.\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;

err:
	if(libtorque_stop()){
		fprintf(stderr,"Error shutting down libtorque.\n");
	}
	return EXIT_FAILURE;
}
