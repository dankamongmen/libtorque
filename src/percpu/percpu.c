#include <stdio.h>
#include <stdlib.h>
#include <libtorque/libtorque.h>

static void
usage(const char *argv0){
	fprintf(stderr,"usage: %s command [ args... ]\n",argv0);
}

int main(int argc,char **argv){
	struct libtorque_ctx *ctx = NULL;

	if(argc == 1){
		usage(*argv);
		return EXIT_FAILURE;
	}
	if((ctx = libtorque_init()) == NULL){
		fprintf(stderr,"Error initializing libtorque.\n");
		goto err;
	}
	if(libtorque_stop(ctx)){
		fprintf(stderr,"Error shutting down libtorque.\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;

err:
	if(libtorque_stop(ctx)){
		fprintf(stderr,"Error shutting down libtorque.\n");
	}
	return EXIT_FAILURE;
}
