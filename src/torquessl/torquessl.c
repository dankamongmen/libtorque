#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <libtorque/ssl/ssl.h>
#include <libtorque/libtorque.h>

static int
ssl_conn_handler(int fd,void *v){
	printf("SSL connection on fd %d, state %p\n",fd,v);
	return 0;
}

static int
make_ssl_fd(int domain,const struct sockaddr *saddr,socklen_t slen){
	int sd;

	if((sd = socket(domain,SOCK_STREAM,0)) < 0){
		return -1;
	}
	if(bind(sd,saddr,slen)){
		close(sd);
		return -1;
	}
	return sd;
}

#define DEFAULT_PORT ((uint16_t)7443)

static void
usage(const char *argv0){
	fprintf(stderr,"usage: %s [ options ]\n",argv0);
	fprintf(stderr,"\t-h: print this message\n");
	fprintf(stderr,"\t-C cafile: provide additional certificate authority\n");
	fprintf(stderr,"\t-k keyfile: provide server key (requires -c)\n");
	fprintf(stderr,"\t-c certfile: provide server cert (requires -k)\n");
}

static int
parse_args(int argc,char **argv,const char **certfile,const char **keyfile,
			const char **cafile){
#define SET_ARG_ONCE(opt,arg) do{ if(!*(arg)){ *arg = optarg; }\
	else{ fprintf(stderr,"Provided '%c' twice\n",(opt)); goto err; }} while(0)

	const char *argv0 = *argv;
	int c;

	while((c = getopt(argc,argv,"c:k:C:h")) > 0){
		switch(c){
		case 'c':
			SET_ARG_ONCE('c',certfile);
			break;
		case 'k':
			SET_ARG_ONCE('k',keyfile);
			break;
		case 'C':
			SET_ARG_ONCE('C',cafile);
			break;
		case 'h':
			usage(argv0);
			exit(EXIT_SUCCESS);
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
	const char *certfile = NULL,*keyfile = NULL,*cafile = NULL;
	struct libtorque_ctx *ctx = NULL;
	struct sockaddr_in sin;
	sigset_t termset;
	SSL_CTX *sslctx;
	int sig,sd = -1;

	sigemptyset(&termset);
	sigaddset(&termset,SIGINT);
	sigaddset(&termset,SIGTERM);
	memset(&sin,0,sizeof(sin));
	if(parse_args(argc,argv,&certfile,&keyfile,&cafile)){
		return EXIT_FAILURE;
	}
	sin.sin_family = AF_INET;
	sin.sin_port = htons(DEFAULT_PORT); // FIXME ugh
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	if(pthread_sigmask(SIG_SETMASK,&termset,NULL)){
		fprintf(stderr,"Couldn't set signal mask\n");
		return EXIT_FAILURE;
	}
	if((ctx = libtorque_init()) == NULL){
		fprintf(stderr,"Couldn't initialize libtorque\n");
		goto err;
	}
	if((sd = make_ssl_fd(AF_INET,(struct sockaddr *)&sin,sizeof(sin))) < 0){
		fprintf(stderr,"Couldn't initialize libtorque\n");
		goto err;
	}
	if(init_ssl()){
		fprintf(stderr,"Couldn't initialize OpenSSL\n");
		goto err;
	}
	if((sslctx = new_ssl_ctx(NULL,NULL,NULL,0))){
		fprintf(stderr,"Couldn't initialize OpenSSL context\n");
		goto err;
	}
	if(libtorque_addssl(ctx,sd,sslctx,ssl_conn_handler,NULL,NULL)){
		fprintf(stderr,"Couldn't add SSL sd %d\n",sd);
		goto err;
	}
	if(sigwait(&termset,&sig)){
		fprintf(stderr,"Couldn't wait on signals\n");
		goto err;
	}
	printf("Got signal %d (%s), closing down...\n",sig,strsignal(sig));
	if(libtorque_stop(ctx)){
		fprintf(stderr,"Couldn't shutdown libtorque\n");
		return EXIT_FAILURE;
	}
	printf("Successfully cleaned up.\n");
	return EXIT_SUCCESS;

err:
	if(libtorque_stop(ctx)){
		fprintf(stderr,"Couldn't shutdown libtorque\n");
		return EXIT_FAILURE;
	}
	if(stop_ssl()){
		fprintf(stderr,"Couldn't shutdown OpenSSL\n");
		return EXIT_FAILURE;
	}
	if(close(sd)){
		fprintf(stderr,"Couldn't close SSL socket %d\n",sd);
		return EXIT_FAILURE;
	}
	return EXIT_FAILURE;
}
