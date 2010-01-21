#include <sys/poll.h>
#include <libtorque/dns/dns.h>
#include <libtorque/internal.h>
#include <libtorque/events/fd.h>

int libtorque_dns_init(dns_state *dctx){
#ifndef LIBTORQUE_WITHOUT_ADNS
	adns_initflags flags = adns_if_debug | adns_if_noautosys;

	if(adns_init(dctx,flags,NULL)){
		return -1;
	}
#else
	if(!dctx){
		return -1;
	}
#endif
	return 0;
}

static void
adns_rx_callback(int fd,void *state){
	struct pollfd pfd = {
		.events = POLLIN|POLLPRI,
		.revents = POLLIN, // FIXME what about POLLPRI (see adns.h)
		.fd = fd,
	};
	struct timeval now;

	printf("RX CALLBACK!\n");
	if(gettimeofday(&now,NULL)){
		// FIXME what?
	}
	if(adns_processany(state)){
		printf("error (%s)\n",strerror(errno));
		return;
	}
	printf("SUCCESS!\n");
	adns_afterpoll(state,&pfd,1,&now); // FIXME add back?
}

static void
adns_tx_callback(int fd,void *state){
	struct pollfd pfd = {
		.events = POLLOUT,
		.revents = POLLOUT,
		.fd = fd,
	};
	struct timeval now;

	printf("TX CALLBACK!\n");
	if(gettimeofday(&now,NULL)){
		// FIXME what?
	}
	adns_afterpoll(state,&pfd,1,&now); // FIXME add back?
}

int load_dns_fds(libtorque_ctx *ctx,dns_state *dctx,const evqueue *evq){
#ifndef LIBTORQUE_WITHOUT_ADNS
	struct pollfd pfds[4];
	int nfds,to = 0,r;

	nfds = sizeof(pfds) / sizeof(*pfds);
	if( (r = adns_beforepoll(ctx->evq.dnsctx,pfds,&nfds,&to,NULL)) ){
		if(r == ERANGE){
			// FIXME go back with more space
		}
		return -1;
	}
	while(nfds--){
		if(add_fd_to_evhandler(ctx,evq,pfds[nfds].fd,
				pfds[nfds].events & (POLLIN | POLLPRI)
					? adns_rx_callback : NULL,
				pfds[nfds].events & POLLOUT
					? adns_tx_callback : NULL,*dctx,EPOLLONESHOT)){
			// FIMXE return -1;
		}
	}
#else
	if(!ctx || !evq || !dctx){
		return -1;
	}
#endif
	return 0;
}

void libtorque_dns_shutdown(dns_state *dctx){
#ifndef LIBTORQUE_WITHOUT_ADNS
	adns_finish(*dctx);
#else
	memset(dctx,0,sizeof(*dctx));
#endif
}
