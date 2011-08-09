#include <sys/poll.h>
#include <libtorque/internal.h>
#include <libtorque/events/fd.h>
#include <libtorque/protos/dns.h>
#include <libtorque/events/thread.h>

int torque_dns_init(dns_state *dctx){
#ifndef LIBTORQUE_WITHOUT_ADNS
	adns_initflags flags = adns_if_noautosys/* | adns_if_debug*/;

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

int restore_dns_fds(dns_state dctx __attribute__ ((unused)),const evhandler *evh){
#ifndef LIBTORQUE_WITHOUT_ADNS
	int nfds,to = 0,r,ret = 0;
	struct pollfd pfds[4];

	// FIXME factor this out, share with load_dns_fds()
	nfds = sizeof(pfds) / sizeof(*pfds);
	if( (r = adns_beforepoll(dctx,pfds,&nfds,&to,NULL)) ){
		if(r == ERANGE){
			// FIXME go back with more space
		}
		return -1;
	}
	while(nfds--){
		int flags = 0;

		flags |= pfds[nfds].events & (POLLIN | POLLPRI) ? EVREAD : 0;
		flags |= pfds[nfds].events & POLLOUT ? EVWRITE : 0;
		ret |= restorefd(evh,fd,flags);
	}
	return ret;
#else
	if(!evh){
		return -1;
	}
	return 0;
#endif
}

typedef struct dnsmarshal {
	libtorquednscb cb;
	void *cbstate;
} dnsmarshal;

dnsmarshal *create_dnsmarshal(libtorquednscb cb,void *cbstate){
	dnsmarshal *ret;

	if( (ret = malloc(sizeof(*ret))) ){
		ret->cb = cb;
		ret->cbstate = cbstate;
	}
	return ret;
}

#ifndef LIBTORQUE_WITHOUT_ADNS
static void
adns_rx_callback(int fd __attribute__ ((unused)),void *state){
	int r;

	if( (r = adns_processany(state)) ){
		return; // FIXME stat? what else?
	}else{
		adns_query query = NULL;
		adns_answer *answer;
		void *context;

		while((r = adns_check(state,&query,&answer,&context)) == 0){
			dnsmarshal *ds = context;

			ds->cb(answer,ds->cbstate);
			free_dnsmarshal(ds);
			free(answer);
			query = NULL;
		}
		if(r != EAGAIN){
			// FIXME what?
		}
	}
	restore_dns_fds(state,get_thread_evh());
}

static void
adns_tx_callback(int fd,void *state){
	struct pollfd pfd = {
		.events = POLLOUT,
		.revents = POLLOUT,
		.fd = fd,
	};
	struct timeval now;

	if(gettimeofday(&now,NULL)){
		// FIXME what?
	}
	adns_afterpoll(state,&pfd,1,&now); // FIXME add back?
}
#endif

int load_dns_fds(torque_ctx *ctx,dns_state *dctx,const evqueue *evq){
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
					? adns_tx_callback : NULL,*dctx,
					EVONESHOT)){
			// FIXME return -1;
		}
	}
#else
	if(!ctx || !evq || !dctx){
		return -1;
	}
#endif
	return 0;
}

void torque_dns_shutdown(dns_state *dctx){
#ifndef LIBTORQUE_WITHOUT_ADNS
	adns_finish(*dctx);
#else
	memset(dctx,0,sizeof(*dctx));
#endif
}
