#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <locale.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <libtorque/torque.h>

static void
print_version(FILE *fp){
	fprintf(fp,"torquehost from libtorque %s\n",torque_version());
}

#define DEFAULT_TIMEOUT 60

static void
usage(const char *argv0){
	fprintf(stderr,"usage:\t%s [ global-opts ] [ query-opts ] query\n",argv0);
	fprintf(stderr,"\t\t\t[ [ query-opts ] query ...]\n");
	fprintf(stderr,"\t%s [ global-opts ] [ query-opts ] -f|--pipe\n",argv0);
	fprintf(stderr,"\nglobal options:\n");
	fprintf(stderr,"\t-t sec: timeout in sec seconds (default: %u, 0 disables)\n",DEFAULT_TIMEOUT);
	fprintf(stderr,"\t-f, --pipe: queries on stdin instead of args\n");
	fprintf(stderr,"\t-h, --help: print this message\n");
	fprintf(stderr,"\t-v, --version: print version info\n");
	//fprintf(stderr,"\nper-query options:\n");
	fprintf(stderr,"\n");
	print_version(stderr);
}

static int
parse_args(int argc,char **argv,FILE **fp,unsigned *timeout){
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

	while((c = getopt_long(argc,argv,"fht:v",opts,NULL)) >= 0){
		switch(c){
		case 't':
			*timeout = atoi(optarg); // FIXME robustify
			break;
		case 'f': case 'h': case 'v':
			lflag = c; // intentional fallthrough
		case 0: // long option
			switch(lflag){
				case 'f':
					SET_ARG_ONCE('f',fp,stdin);
					break;
				case 'h':
					usage(argv0);
					exit(EXIT_SUCCESS);
				case 'v':
					print_version(stdout);
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

// Don't allow the resolution callback to terminate us before we've registered
// all the queries we intend to run.
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void
timeoutcb(void *state __attribute__ ((unused))){
	printf("TIMEOUT!\n");
	abort();
}

static void
lookup_callback(const torque_dnsret *dnsret,void *state){
	char ipbuf[INET_ADDRSTRLEN];
	int returns,*shared_returns;

	if(dnsret->status){
		fprintf(stderr,"resolution error %d (%s)\n",dnsret->status,
					adns_strerror(dnsret->status));
	}else if(inet_ntop(AF_INET,&dnsret->rrs.inaddr->s_addr,ipbuf,sizeof(ipbuf)) == NULL){
		fprintf(stderr,"inet_ntop(%ju) failed (%s)\n",
			(uintmax_t)dnsret->rrs.inaddr->s_addr,strerror(errno));
	}else{
		printf("%s\n",ipbuf);
	}
	pthread_mutex_lock(&lock);
	shared_returns = state;
	returns = --*shared_returns;
	pthread_mutex_unlock(&lock);
	if(returns <= 0){
		if(returns < 0){
			fprintf(stderr,"got %d extra returns\n",-returns);
		}else{
			if(isatty(fileno(stdout))){
				printf("Got all returns\n");
			}
		}
		pthread_cond_signal(&cond);
	}
}

static int
add_lookup(struct torque_ctx *ctx,const char *host,int *results){
	torque_err err;

	if(pthread_mutex_lock(&lock)){
		return -1;
	}
	++*results;
	if(pthread_mutex_unlock(&lock)){
		return -1;
	}
	if( (err = torque_addlookup_dns(ctx,host,lookup_callback,results)) ){
		fprintf(stderr,"Error adding query (%s)\n",torque_errstr(err));
		return -1;
	}
	return 0;
}

static char *
fpgetline(FILE *fp){
	char line[80],*ret;

	// FIXME all broken
	while(fgets(line,sizeof(line),fp)){
		char *nl;

		if((nl = strchr(line,'\n')) == NULL){
			return NULL;
		}
		*nl = '\0';
		ret = strdup(line);
		return ret;
	}
	return NULL;
}

static int
spool_targets(struct torque_ctx *ctx,FILE *fp,char **argv,int *results){
	*results = 1;
	if(fp){
		char *l;

		while(errno = 0, (l = fpgetline(fp)) ){
			if(add_lookup(ctx,l,results)){
				free(l);
				return -1;
			}
			free(l);
		}
		if(errno || ferror(fp)){ // errno for ENOMEM check
			fprintf(stderr,"Error reading from input (%s?)\n",strerror(errno));
			return -1;
		}
	}else{
		argv += optind;
		while(*argv){
			if(add_lookup(ctx,*argv,results)){
				return -1;
			}
			++argv;
		}
	}
	return 0;
}

int main(int argc,char **argv){
	unsigned timeout = DEFAULT_TIMEOUT;
	struct torque_ctx *ctx = NULL;
	const char *a0 = *argv;
	torque_err err;
	FILE *fp = NULL;
	int results;

	if(setlocale(LC_ALL,"") == NULL){
		fprintf(stderr,"Couldn't set locale\n");
		goto err;
	}
	if(parse_args(argc,argv,&fp,&timeout)){
		fprintf(stderr,"Error parsing arguments\n");
		usage(a0);
		goto err;
	}
	if(fp && argv[optind]){
		fprintf(stderr,"Query arguments provided with -f/--pipe\n");
		usage(a0);
		goto err;
	}else if(!fp && !argv[optind]){
		fprintf(stderr,"No query arguments provided, no -f/--pipe\n");
		usage(a0);
		goto err;
	}
	if((ctx = torque_init(&err)) == NULL){
		fprintf(stderr,"Couldn't initialize libtorque (%s)\n",
				torque_errstr(err));
		goto err;
	}
	if(timeout){
		struct itimerspec it;

		memset(&it,0,sizeof(it)); // FIXME
		it.it_interval.tv_sec = timeout;
		if( (err = torque_addtimer(ctx,&it,timeoutcb,NULL)) ){
			fprintf(stderr,"Couldn't add timer (%s)\n",
					torque_errstr(err));
			goto err;
		}
	}
	if(spool_targets(ctx,fp,argv,&results)){
		usage(a0);
		goto err;
	}
	if(isatty(fileno(stdout))){
		if(timeout){
			printf("Waiting %us for resolutions...\n",timeout);
		}else{
			printf("Waiting for resolutions, timeouts disabled...\n");
		}
	}
	pthread_mutex_lock(&lock);
	--results;
	while(results){
		pthread_cond_wait(&cond,&lock);
	}
	pthread_mutex_unlock(&lock);
	if( (err = torque_stop(ctx)) ){
		fprintf(stderr,"Couldn't stop libtorque (%s)\n",
				torque_errstr(err));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;

err:
	if( (err = torque_stop(ctx)) ){
		fprintf(stderr,"Couldn't destroy libtorque (%s)\n",
				torque_errstr(err));
	}
	return EXIT_FAILURE;
}
