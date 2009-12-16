#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <libtorque/buffers.h>
#include <libtorque/libtorque.h>

static int
signalrx(int sig,struct libtorque_cbctx *cbctx,void *v __attribute__ ((unused))){
	printf("received signal %d %p\n",sig,cbctx);
	return 0;
}

static void
print_version(void){
	fprintf(stderr,"signalrx from libtorque " LIBTORQUE_VERSIONSTR "\n");
}

static void
usage(const char *argv0){
	fprintf(stderr,"usage: %s [ options ]\n",argv0);
	fprintf(stderr,"available options:\n");
	fprintf(stderr,"\t--version: print version info\n");
}

static int
parse_args(int argc,char **argv){
#define SET_ARG_ONCE(opt,arg,val) do{ if(!*(arg)){ *arg = val; }\
	else{ fprintf(stderr,"Provided '%c' twice\n",(opt)); goto err; }} while(0)
	int lflag;
	const struct option opts[] = {
		{	 .name = "version",
			.has_arg = 0,
			.flag = &lflag,
			.val = 'v',
		},
		{	 .name = NULL, .has_arg = 0, .flag = 0, .val = 0, },
	};
	const char *argv0 = *argv;
	int c;

	while((c = getopt_long(argc,argv,"",opts,NULL)) >= 0){
		switch(c){
		case 0: // long option
			switch(lflag){
				case 'v':
					print_version();
					exit(EXIT_SUCCESS);
				default:
					goto err;
			}
		default:
			goto err;
		}
	}
	return 0;

err:
	usage(argv0);
	return -1;
#undef SET_ARG_ONCE
}

static const int signals_watched[] = {
	SIGHUP,
	SIGPIPE,
	SIGALRM,
	SIGUSR1,
	SIGUSR2,
	SIGCHLD,
	SIGPOLL,
	SIGIO,
	SIGURG,
	SIGWINCH,
};

int main(int argc,char **argv){
	struct libtorque_ctx *ctx = NULL;
	sigset_t ss;
	unsigned z;

	if(parse_args(argc,argv)){
		return EXIT_FAILURE;
	}
	if((ctx = libtorque_init()) == NULL){
		fprintf(stderr,"Couldn't initialize libtorque\n");
		goto err;
	}
	if(sigemptyset(&ss)){
		fprintf(stderr,"Couldn't initialize sigset\n");
		goto err;
	}
	for(z = 0 ; z < sizeof(signals_watched) / sizeof(*signals_watched) ; ++z){
		int s = signals_watched[z];

		if(sigaddset(&ss,s)){
			fprintf(stderr,"Couldn't add signal %d to set\n",signals_watched[z]);
			goto err;
		}
		printf("Watching signal %d (%s)\n",s,strsignal(s));
	}
	if(libtorque_addsignal(ctx,&ss,signalrx,NULL)){
		fprintf(stderr,"Couldn't listen on signals\n");
		goto err;
	}
	if(libtorque_block(ctx)){
		fprintf(stderr,"Couldn't block on libtorque\n");
		return EXIT_FAILURE;
	}
	printf("Successfully cleaned up.\n");
	return EXIT_SUCCESS;

err:
	if(libtorque_stop(ctx)){
		fprintf(stderr,"Couldn't shutdown libtorque\n");
		return EXIT_FAILURE;
	}
	return EXIT_FAILURE;
}
