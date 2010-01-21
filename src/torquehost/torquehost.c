#include <stdio.h>
#include <errno.h>
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
	fprintf(stderr,"\t-f, --pipe: queries on stdin instead of args\n");
	fprintf(stderr,"\t-h, --help: print this message\n");
	fprintf(stderr,"\t-v, --version: print version info\n");
}

static int
parse_args(int argc,char **argv,FILE **fp){
#define SET_ARG_ONCE(opt,arg,val) do{ if(!*(arg)){ *arg = val; }\
	else{ fprintf(stderr,"Provided '%c' twice\n",(opt)); goto err; }} while(0)
	int lflag;
	const struct option opts[] = {
		{	.name = "pipe",
			.has_arg = 0,
			.flag = &lflag,
			.val = 'f',
		},
		{	.name = "help",
			.has_arg = 0,
			.flag = &lflag,
			.val = 'h',
		},
		{	.name = "version",
			.has_arg = 0,
			.flag = &lflag,
			.val = 'v',
		},
		{	 .name = NULL, .has_arg = 0, .flag = 0, .val = 0, },
	};
	const char *argv0 = *argv;
	int c;

	while((c = getopt_long(argc,argv,"hvf",opts,NULL)) >= 0){
		switch(c){
		case 'f': case 'h': case 'v':
			lflag = c; // intentional fallthrough
		case 0: // long option
			switch(lflag){
				case 'f':
					SET_ARG_ONCE('f',fp,stdin);
					break;
				case 'v':
					print_version();
					exit(EXIT_SUCCESS);
				case 'h':
					usage(argv0);
					exit(EXIT_SUCCESS);
				default:
					return -1;
			}
			break;
		default:
			return -1;
		}
	}
	return 0;
#undef SET_ARG_ONCE

err:
	return -1;
}

static int
add_lookup(struct libtorque_ctx *ctx,const char *host){
	printf("%p %p\n",ctx,host); // FIXME
	return 0;
}

static char *
fpgetline(FILE *fp){
	char line[80],*ret;

	// FIXME
	while(fgets(line,sizeof(line),fp)){
		ret = strdup(line);
		return ret;
	}
	return NULL;
}

static int
spool_targets(struct libtorque_ctx *ctx,FILE *fp,char **argv){
	if(fp){
		char *l;

		if(argv[optind] != NULL){
			fprintf(stderr,"Query arguments provided with -f/--pipe\n");
			return -1;
		}
		errno = 0;
		while( (l = fpgetline(fp)) ){
			if(add_lookup(ctx,l)){
				free(l);
				return -1;
			}
			free(l);
		}
		if(errno || ferror(fp)){ // errno for ENOMEM check
			return -1;
		}
	}else{
		if(argv[optind] == NULL){
			fprintf(stderr,"No query arguments provided, no -f/--pipe\n");
			return -1;
		}
		argv += optind;
		while(*argv){
			if(add_lookup(ctx,*argv)){
				return -1;
			}
			++argv;
		}
	}
	return 0;
}

int main(int argc,char **argv){
	struct libtorque_ctx *ctx = NULL;
	const char *a0 = *argv;
	libtorque_err err;
	FILE *fp = NULL;

	if(setlocale(LC_ALL,"") == NULL){
		fprintf(stderr,"Couldn't set locale\n");
		goto err;
	}
	if(parse_args(argc,argv,&fp)){
		fprintf(stderr,"Error parsing arguments\n");
		usage(a0);
		goto err;
	}
	if((ctx = libtorque_init(&err,NULL)) == NULL){
		fprintf(stderr,"Couldn't initialize libtorque (%s)\n",
				libtorque_errstr(err));
		goto err;
	}
	if(spool_targets(ctx,fp,argv)){
		usage(a0);
		goto err;
	}
	// FIXME only print this if we're interactive
	printf("Waiting for resolutions, press Ctrl+c to interrupt...\n");
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
