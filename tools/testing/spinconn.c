#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <libtorque/torque.h>

#define DEFAULT_PORT ((uint16_t)4007)

static void
print_version(void){
	fprintf(stderr,"spinconn from libtorque %s\n",torque_version());
}

static void
usage(const char *argv0){
	fprintf(stderr,"usage: %s [ options ]\n",argv0);
	fprintf(stderr,"available options:\n");
	fprintf(stderr,"\t-t, --tcp port: specify TCP port to target (default: %hu)\n",DEFAULT_PORT);
	fprintf(stderr,"\t-u, --udp port: specify UDP port to target\n");
	fprintf(stderr,"\t-s, --ssl port: specify TCP port to target using SSL\n");
	fprintf(stderr,"\t-v, --version: print version info\n");
	fprintf(stderr,"\t-h, --help: print this message\n");
}

typedef int (*fdmaker)(void);

static inline int
nb_socket(int domain,int type){
	int fd;

	if((fd = socket(domain,type,0)) >= 0){
		int flags;

		if((flags = fcntl(fd,F_GETFL)) < 0 || fcntl(fd,F_SETFL,flags | O_NONBLOCK)){
			close(fd);
			fd = -1;
		}
	}
	return fd;
}

static int
sslmaker(void){
	return -1; // FIXME
}

static int
udpmaker(void){
	return nb_socket(AF_INET,SOCK_DGRAM);
}

static int
tcpmaker(void){
	return nb_socket(AF_INET,SOCK_STREAM);
}

static int
parse_args(int argc,char **argv,uint16_t *port,fdmaker *fdfxn){
#define SET_ARG_ONCE(opt,arg,val) do{ if(!*(arg)){ *arg = val; }\
	else{ fprintf(stderr,"Provided '%c' twice\n",(opt)); goto err; }} while(0)
	int lflag;
	const struct option opts[] = {
		{	 .name = "help",
			.has_arg = 0,
			.flag = &lflag,
			.val = 'h',
		},
		{	 .name = "ssl",
			.has_arg = 0,
			.flag = &lflag,
			.val = 's',
		},
		{	 .name = "tcp",
			.has_arg = 0,
			.flag = &lflag,
			.val = 't',
		},
		{	 .name = "udp",
			.has_arg = 0,
			.flag = &lflag,
			.val = 'u',
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

	while((c = getopt_long(argc,argv,"hs:t:u:v",opts,NULL)) >= 0){
		switch(c){
		case 'h': case 's': case 't': case 'u':
			  lflag = c;
			  // intentional fall-through
		case 0: // long option
			switch(lflag){
				case 'h':
					usage(argv0);
					exit(EXIT_SUCCESS);
				case 's': { int p = atoi(optarg);
					if(p > 0xffff || p == 0){
						goto err;
					}
					SET_ARG_ONCE('s',port,p);
					*fdfxn = sslmaker;
					break;
				}
				case 't': { int p = atoi(optarg);
					if(p > 0xffff || p == 0){
						goto err;
					}
					SET_ARG_ONCE('t',port,p);
					*fdfxn = tcpmaker;
					break;
				}
				case 'u': { int p = atoi(optarg);
					if(p > 0xffff || p == 0){
						goto err;
					}
					SET_ARG_ONCE('u',port,p);
					*fdfxn = udpmaker;
					break;
				}
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
	*port = htons(*port ? *port : DEFAULT_PORT);
	*fdfxn = *fdfxn ? *fdfxn : tcpmaker;
	return 0;

err:
	usage(argv0);
	return -1;
#undef SET_ARG_ONCE
}

typedef struct spinconnctx {
	struct torque_ctx *ctx;
	const struct sockaddr *sa;
	socklen_t slen;
	fdmaker fdfxn;
} spinconnctx;

static void
conn_complete_cb(int fd,void *state){
	spinconnctx *ctx = state;
	int sd;

	close(fd);
	while((sd = ctx->fdfxn()) >= 0){
		torque_err err;

		if( (err = torque_addconnector_unbuffered(ctx->ctx,sd,
				ctx->sa,ctx->slen,NULL,conn_complete_cb,ctx)) ){
			fprintf(stderr,"Couldn't add descriptor %d (%s)\n",sd,
					torque_errstr(err));
			close(sd);
			break;
		}
	}
}
 
int main(int argc,char **argv){
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
	} su;
	spinconnctx ctx = {
		.ctx = NULL,
		.fdfxn = NULL,
	};
	torque_err err;
	int sd = -1;

	memset(&su,0,sizeof(su));
	if(parse_args(argc,argv,&su.sin.sin_port,&ctx.fdfxn)){
		return EXIT_FAILURE;
	}
	if( (err = torque_sigmask(NULL)) ){
		fprintf(stderr,"Couldn't shutdown libtorque (%s)\n",
				torque_errstr(err));
		return EXIT_FAILURE;
	}
	if((ctx.ctx = torque_init(&err)) == NULL){
		fprintf(stderr,"Couldn't initialize libtorque (%s)\n",
				torque_errstr(err));
		goto err;
	}
	su.sin.sin_family = AF_INET;
	su.sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if((sd = ctx.fdfxn()) < 0){
		fprintf(stderr,"Couldn't get socket (%s)\n",
				strerror(errno));
		goto err;
	}
	printf("Spinning connections to port %hu...\n",ntohs(su.sin.sin_port));
	ctx.sa = &su.sa;
	ctx.slen = sizeof(su.sin);
	if( (err = torque_addconnector_unbuffered(ctx.ctx,sd,ctx.sa,
				ctx.slen,NULL,conn_complete_cb,&ctx)) ){
		fprintf(stderr,"Couldn't add descriptor %d (%s)\n",sd,
				torque_errstr(err));
		goto err;
	}
	if( (err = torque_block(ctx.ctx)) ){
		fprintf(stderr,"Couldn't shutdown libtorque (%s)\n",
				torque_errstr(err));
		return EXIT_FAILURE;
	}
	printf("Successfully cleaned up.\n");
	return EXIT_SUCCESS;

err:
	if( (err = torque_stop(ctx.ctx)) ){
		fprintf(stderr,"Couldn't shutdown libtorque (%s)\n",
				torque_errstr(err));
		return EXIT_FAILURE;
	}
	if((sd >= 0) && close(sd)){
		fprintf(stderr,"Couldn't close server socket %d\n",sd);
		return EXIT_FAILURE;
	}
	return EXIT_FAILURE;
}
