#include <stdio.h>
#include <locale.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <libtorque/libtorque.h>

static void
print_version(void){
	fprintf(stderr,"torquehost from libtorque " LIBTORQUE_VERSIONSTR "\n");
}

static void
usage(const char *argv0){
	fprintf(stderr,"usage: %s [ options ]\n",argv0);
	fprintf(stderr,"\t-h, --help: print this message\n");
	fprintf(stderr,"\t-v, --version: print version info\n");
}

static int
parse_args(int argc,char **argv){
	int lflag;
	const struct option opts[] = {
		{	 .name = "help",
			.has_arg = 0,
			.flag = &lflag,
			.val = 'h',
		},
		{	 .name = "version",
			.has_arg = 0,
			.flag = &lflag,
			.val = 'v',
		},
		{	 .name = NULL, .has_arg = 0, .flag = 0, .val = 0, },
	};
	const char *argv0 = *argv;
	int c;

	while((c = getopt_long(argc,argv,"hv",opts,NULL)) >= 0){
		switch(c){
		case 'h': case 'v':
			lflag = c; // intentional fallthrough
		case 0: // long option
			switch(lflag){
				case 'v':
					print_version();
					exit(EXIT_SUCCESS);
				case 'h':
					usage(argv0);
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
	const char *a0 = *argv;
	libtorque_err err;

	if(setlocale(LC_ALL,"") == NULL){
		fprintf(stderr,"Couldn't set locale\n");
		goto err;
	}
	if(parse_args(argc,argv)){
		fprintf(stderr,"Error parsing arguments\n");
		usage(a0);
		goto err;
	}
	if((ctx = libtorque_init(&err,NULL)) == NULL){
		fprintf(stderr,"Couldn't initialize libtorque (%s)\n",
				libtorque_errstr(err));
		goto err;
	}
	if( (err = libtorque_block(ctx)) ){
		fprintf(stderr,"Couldn't block on libtorque (%s)\n",
				libtorque_errstr(err));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;

err:
	if( (err = libtorque_stop(ctx)) ){
		fprintf(stderr,"Couldn't destroy libtorque (%s)\n",
				libtorque_errstr(err));
	}
	return EXIT_FAILURE;
}
