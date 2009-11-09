#include <stdio.h>
#include <stdlib.h>

static void
usage(const char *argv0){
	fprintf(stderr,"usage: %s command [ args... ]\n",argv0);
}

int main(int argc,char **argv){
	if(argc == 1){
		usage(*argv);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
