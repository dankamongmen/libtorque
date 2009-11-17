#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <libtorque/libtorque.h>

static void
print_version(void){
	fprintf(stderr,"percpu from libtorque " LIBTORQUE_VERSIONSTR "\n");
}

static void
usage(const char *argv0){
	fprintf(stderr,"usage: %s [ options ]\n",argv0);
	fprintf(stderr,"\t-h: print this message\n");
	fprintf(stderr,"\t--version: print version info\n");
}

static int
parse_args(int argc,char **argv){
	int lflag;
	const struct option opts[] = {
		{	.name = "version",
			.has_arg = 0,
			.flag = &lflag,
			.val = 'v',
		},
		{	 .name = NULL, .has_arg = 0, .flag = 0, .val = 0, },
	};
	const char *argv0 = *argv;
	int c;

	while((c = getopt_long(argc,argv,"h",opts,NULL)) >= 0){
		switch(c){
		case 'h':
			usage(argv0);
			exit(EXIT_SUCCESS);
		case 0: // long option
			switch(lflag){
				case 'v':
					print_version();
					exit(EXIT_SUCCESS);
				default:
					return -1;
			}
		default:
			return -1;
		}
	}
	return 0;
}

int main(int argc,char **argv){
	struct libtorque_ctx *ctx = NULL;
	const char *ag0 = *argv;

	if(argc == 1){
		usage(*argv);
		return EXIT_FAILURE;
	}
	if(parse_args(argc,argv)){
		fprintf(stderr,"Error parsing arguments.\n");
		usage(ag0);
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
