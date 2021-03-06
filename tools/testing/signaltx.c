#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <limits.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

#define DEFAULT_SIG SIGTERM

static volatile int stopsending;

static void
rxtermsig(int s){
	stopsending = s;
}

static void
print_version(void){
	fprintf(stderr,"signaltx from libtorque " TORQUE_VERSIONSTR "\n");
}

static void
usage(const char *argv0){
	fprintf(stderr,"usage: %s [ options ] pid\n",argv0);
	fprintf(stderr,"available options:\n");
	fprintf(stderr,"\t-c, --count count: number of signals (default: 0 (continuous))\n");
	fprintf(stderr,"\t-s, --sig signum: specify signal (default: %d (%s))\n",
			DEFAULT_SIG,strsignal(DEFAULT_SIG));
	fprintf(stderr,"\t--version: print version info\n");
}

static int
parse_args(int argc,char **argv,pid_t *pid,int *sig,uintmax_t *count){
#define SET_ARG_ONCE(opt,arg,val) do{ if(!*(arg)){ *arg = val; }\
	else{ fprintf(stderr,"Provided '%c' twice\n",(opt)); goto err; }} while(0)
	int lflag,c;
	const struct option opts[] = {
		{	.name = "version",
			.has_arg = 0,
			.flag = &lflag,
			.val = 'v',
		},
		{	.name = "count",
			.has_arg = 1,
			.flag = &lflag,
			.val = 'c',
		},
		{	.name = "sig",
			.has_arg = 1,
			.flag = &lflag,
			.val = 's',
		},
		{	 .name = NULL, .has_arg = 0, .flag = 0, .val = 0, },
	};
	const char *argv0 = *argv;
	char *pidstr;
	long r;

	*sig = 0;
	*count = 0;
	while((c = getopt_long(argc,argv,"c:s:",opts,NULL)) >= 0){
		switch(c){
		case 'c': case 's':
			lflag = c;
			// intentional fall-through
		case 0: // long option
			switch(lflag){
				case 'c':
					SET_ARG_ONCE('c',count,atoi(optarg));
					break;
				case 's':
					SET_ARG_ONCE('s',sig,atoi(optarg));
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
	r = strtol(argv[optind],&pidstr,10);
	if(r <= 0 || (r == LONG_MAX && errno == ERANGE) ||
			argv[optind][0] == '\0' || *pidstr != '\0'){
		fprintf(stderr,"Couldn't use PID: %s\n",argv[optind]);
		goto err;
	}
	*pid = r;
	if(*sig == 0){
		*sig = DEFAULT_SIG;
	}
	return 0;

err:
	usage(argv0);
	return -1;
#undef SET_ARG_ONCE
}

int main(int argc,char **argv){
	uintmax_t count,sent;
	struct sigaction sa;
	pid_t pid;
	int sig;

	if(parse_args(argc,argv,&pid,&sig,&count)){
		return EXIT_FAILURE;
	}
	memset(&sa,0,sizeof(sa));
	sa.sa_handler = rxtermsig;
	if(sigaction(SIGINT,&sa,NULL) || sigaction(SIGTERM,&sa,NULL)){
		fprintf(stderr,"Couldn't install signal handlers\n");
		return EXIT_FAILURE;
	}
	printf("Using signal %d (%s)...",sig,strsignal(sig));
	fflush(stdout);
	sent = 0;
	while(!stopsending){
		if(kill(pid,sig)){
			fprintf(stderr,"\nError sending signal %d (%s) (#%ju)\n",
					sig,strerror(errno),sent + 1);
			goto done;
		}
		if(++sent == count){
			printf("done.\n");
			break;
		}
	}
	if(stopsending){
		printf("got %s.\n",strsignal(stopsending));
	}

done:
	printf("Sent %ju signal%s.\n",sent,sent == 1 ? "" : "s");
	return EXIT_SUCCESS;
}
