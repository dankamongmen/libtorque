#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <libtorque/buffers.h>
#include <libtorque/libtorque.h>

static struct {
	pthread_mutex_t lock;
	const int sig;
	uint64_t rx;
} signals_watched[] = {
	{ PTHREAD_MUTEX_INITIALIZER, SIGHUP, 0 },
	{ PTHREAD_MUTEX_INITIALIZER, SIGPIPE, 0 },
	{ PTHREAD_MUTEX_INITIALIZER, SIGALRM, 0 },
	{ PTHREAD_MUTEX_INITIALIZER, SIGUSR1, 0 },
	{ PTHREAD_MUTEX_INITIALIZER, SIGUSR2, 0 },
	{ PTHREAD_MUTEX_INITIALIZER, SIGCHLD, 0 },
	{ PTHREAD_MUTEX_INITIALIZER, SIGPOLL, 0 },
	{ PTHREAD_MUTEX_INITIALIZER, SIGIO, 0 },
	{ PTHREAD_MUTEX_INITIALIZER, SIGURG, 0 },
	{ PTHREAD_MUTEX_INITIALIZER, SIGWINCH, 0 },
};

static void
signalrx(int sig,struct libtorque_cbctx *cbctx __attribute__ ((unused)),
				void *v __attribute__ ((unused))){
	unsigned z;

	for(z = 0 ; z < sizeof(signals_watched) / sizeof(*signals_watched) ; ++z){
		if(signals_watched[z].sig == sig){
			pthread_mutex_lock(&signals_watched[z].lock);
				++signals_watched[z].rx;
			pthread_mutex_unlock(&signals_watched[z].lock);
			break;
		}
	}
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
			break;
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

int main(int argc,char **argv){
	struct libtorque_ctx *ctx = NULL;
	uintmax_t totalsigs;
	libtorque_err err;
	sigset_t ss;
	unsigned z;

	if(parse_args(argc,argv)){
		return EXIT_FAILURE;
	}
	if((ctx = libtorque_init(&err)) == NULL){
		fprintf(stderr,"Couldn't initialize libtorque (%s)\n",
				libtorque_errstr(err));
		goto err;
	}
	if(sigemptyset(&ss)){
		fprintf(stderr,"Couldn't initialize sigset\n");
		goto err;
	}
	for(z = 0 ; z < sizeof(signals_watched) / sizeof(*signals_watched) ; ++z){
		int s = signals_watched[z].sig;

		if(sigismember(&ss,s)){ // some are duplicates
			continue;
		}
		if(sigaddset(&ss,s)){
			fprintf(stderr,"Couldn't add signal %d to set\n",s);
			goto err;
		}
		printf("Watching signal %d (%s)\n",s,strsignal(s));
	}
	if(libtorque_addsignal(ctx,&ss,signalrx,NULL)){
		fprintf(stderr,"Couldn't listen on signals\n");
		goto err;
	}
	if( (err = libtorque_block(ctx)) ){
		fprintf(stderr,"Error blocking on libtorque (%s)\n",
				libtorque_errstr(err));
		goto err;
	}
	totalsigs = 0;
	for(z = 0 ; z < sizeof(signals_watched) / sizeof(*signals_watched) ; ++z){
		int s = signals_watched[z].sig;

		if(signals_watched[z].rx){
			printf("Received signal %d (%s) %ju time%s\n",
					s,strsignal(s),signals_watched[z].rx,
					signals_watched[z].rx == 1 ? "" : "s");
			totalsigs += signals_watched[z].rx;
		}
	}
	if(!totalsigs){
		fprintf(stderr,"Received no signals.\n");
		return EXIT_FAILURE;
	}
	printf("Received %ju total signal%s.\n",totalsigs,totalsigs == 1 ? "" : "s");
	return EXIT_SUCCESS;

err:
	if( (err = libtorque_stop(ctx)) ){
		fprintf(stderr,"Couldn't shutdown libtorque (%s)\n",
				libtorque_errstr(err));
		return EXIT_FAILURE;
	}
	return EXIT_FAILURE;
}
