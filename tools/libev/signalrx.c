#include <ev.h>
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
	{ PTHREAD_MUTEX_INITIALIZER, SIGTERM, 0 },
	{ PTHREAD_MUTEX_INITIALIZER, SIGINT, 0 },
#ifdef SIGPOLL
	{ PTHREAD_MUTEX_INITIALIZER, SIGPOLL, 0 },
#endif
	{ PTHREAD_MUTEX_INITIALIZER, SIGIO, 0 },
	{ PTHREAD_MUTEX_INITIALIZER, SIGURG, 0 },
	{ PTHREAD_MUTEX_INITIALIZER, SIGWINCH, 0 },
};

static void
rxsignal(struct ev_loop *loop,ev_signal *w,int revents __attribute__ ((unused))){
	int sig = w->signum;
	unsigned z;

	if(sig == SIGINT){
		ev_unloop(loop,EVUNLOOP_ALL);
	}
	for(z = 0 ; z < sizeof(signals_watched) / sizeof(*signals_watched) ; ++z){
		if(signals_watched[z].sig == sig){
			pthread_mutex_lock(&signals_watched[z].lock);
				++signals_watched[z].rx;
			pthread_mutex_unlock(&signals_watched[z].lock);
			break;
		}
	}
	// FIXME on SIGTERM, do a controlled exit (if possible)
}

static void
print_version(void){
	fprintf(stderr,"libev-signalrx from libtorque " LIBTORQUE_VERSIONSTR "\n");
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
	struct ev_loop *loop = NULL;
	int ret = EXIT_FAILURE;
	unsigned z;

	if(parse_args(argc,argv)){
		return EXIT_FAILURE;
	}
	if((loop = ev_default_loop(0)) == NULL){
		fprintf(stderr,"Couldn't initialize libev\n");
		goto err;
	}
	/*for(z = 0 ; z < sizeof(signals_watched) / sizeof(*signals_watched) ; ++z){
		int s = signals_watched[z].sig;
		ev_signal evs;

		// FIXME libev requires one loop per signal, aieee
		ev_signal_init(&evs,rxsignal,s);
		ev_signal_start(loop,&evs);
		printf("Watching signal %d (%s)\n",s,strsignal(s));
	}*/
	{ // FIXME
	ev_signal evs;
	ev_signal_init(&evs,rxsignal,SIGINT);
	ev_signal_start(loop,&evs);
	printf("Watching signal %d (%s)\n",SIGINT,strsignal(SIGINT));
	}
	ev_loop(loop,0);
	for(z = 0 ; z < sizeof(signals_watched) / sizeof(*signals_watched) ; ++z){
		int s = signals_watched[z].sig;

		if(signals_watched[z].rx){
			printf("Received signal %d (%s) %ju time%s\n",
					s,strsignal(s),signals_watched[z].rx,
					signals_watched[z].rx == 1 ? "" : "s");
			ret = EXIT_SUCCESS;
		}
	}
	if(ret != EXIT_SUCCESS){
		fprintf(stderr,"Received no signals.\n");
	}
	printf("Successfully cleaned up.\n");
	return ret;

err:
	printf("Shutting down libev...");
	fflush(stdout);
	ev_default_destroy();
	printf("done.\n");
	return EXIT_FAILURE;
}
