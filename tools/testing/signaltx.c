#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

static void
print_version(void){
	fprintf(stderr,"signaltx from libtorque " LIBTORQUE_VERSIONSTR "\n");
}

static void
usage(const char *argv0){
	fprintf(stderr,"usage: %s [ options ] target\n",argv0);
	fprintf(stderr,"available options:\n");
	fprintf(stderr,"\t-p, --pid: target specifies a pid\n");
	fprintf(stderr,"\t--version: print version info\n");
}

static int
parse_args(int argc,char **argv,pid_t *pid){
#define SET_ARG_ONCE(opt,arg,val) do{ if(!*(arg)){ *arg = val; }\
	else{ fprintf(stderr,"Provided '%c' twice\n",(opt)); goto err; }} while(0)
	int lflag,pidtarget = 0;
	const struct option opts[] = {
		{	 .name = "version",
			.has_arg = 0,
			.flag = &lflag,
			.val = 'v',
		},
		{	 .name = "pid",
			.has_arg = 0,
			.flag = &lflag,
			.val = 'p',
		},
		{	 .name = NULL, .has_arg = 0, .flag = 0, .val = 0, },
	};
	const char *argv0 = *argv;
	int c;

	while((c = getopt_long(argc,argv,"p",opts,NULL)) >= 0){
		switch(c){
		case 'p':
			SET_ARG_ONCE('p',&pidtarget,1);
			break;
		case 0: // long option
			switch(lflag){
				case 'p':
					SET_ARG_ONCE('p',&pidtarget,1);
					break;
				case 'v':
					print_version();
					exit(EXIT_SUCCESS);
				default:
					goto err;
			}
			break;
		default:
			goto err;
		}
	}
	if(argv[optind] == NULL || argv[optind + 1] != NULL){
		goto err; // require a target and no other params
	}
	if(pidtarget){
		*pid = atoi(argv[optind]); // FIXME error-checking
	}else{
		goto err; // FIXME implement
	}
	return 0;

err:
	usage(argv0);
	return -1;
#undef SET_ARG_ONCE
}

int main(int argc,char **argv){
	int sig = SIGUSR1;
	pid_t pid;

	if(parse_args(argc,argv,&pid)){
		return EXIT_FAILURE;
	}
	for( ; ; ){
		if(kill(pid,sig)){
			fprintf(stderr,"Error sending signal %d\n",sig);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
