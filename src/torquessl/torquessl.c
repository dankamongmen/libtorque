#include <stdio.h>
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
	if(fd < 0){
		return -1;
	}
	if(v == NULL){
		return -1;
	}
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

int main(void){
	struct libtorque_ctx *ctx = NULL;
	struct sockaddr_in sin;
	sigset_t termset;
	SSL_CTX *sslctx;
	int sig,sd = -1;

	sigemptyset(&termset);
	sigaddset(&termset,SIGINT);
	sigaddset(&termset,SIGTERM);
	memset(&sin,0,sizeof(sin));
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
