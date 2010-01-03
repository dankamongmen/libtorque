#include <errno.h>
#include <unistd.h>
#include <libtorque/buffers.h>
#include <libtorque/libtorque.h>
#include <libtorque/events/thread.h>

static inline int
callback(libtorque_rxbuf *rxb,int fd,libtorque_cbctx *cbctx,void *cbstate){
	if(rxb->bufoff - rxb->bufate){
		return ((const libtorquercb)cbctx->cbstate)(fd,cbctx,cbstate);
	}
	return 0;
}

int buffered_rxfxn(int fd,libtorque_cbctx *cbctx,void *cbstate){
	libtorque_rxbuf *rxb = cbctx->rxbuf;
	int r;

	for( ; ; ){
		if(rxb->buftot - rxb->bufoff == 0){
			if(callback(rxb,fd,cbctx,cbstate)){
				break;
			}
			if(rxb->buftot - rxb->bufoff == 0){
				// FIXME no space cleared; grow that fucker
				break;
			}
		}
		if((r = read(fd,rxb->buffer + rxb->bufoff,rxb->buftot - rxb->bufoff)) > 0){
			rxb->bufoff += r;
		}else if(r == 0){
			if(callback(rxb,fd,cbctx,cbstate)){
				break;
			}
			break; // FIXME must close, *unless* TX indicated
		}else if(errno == EAGAIN || errno == EWOULDBLOCK){
			struct epoll_event ee;
			evhandler *evh;

			if(callback(rxb,fd,cbctx,cbstate)){
				break;
			}
			evh = get_thread_evh();
			memset(&ee,0,sizeof(ee));
			ee.events = EPOLLIN | EPOLLRDHUP | EPOLLET | EPOLLONESHOT;
			ee.data.fd = fd;
			if(epoll_ctl(evh->evq->efd,EPOLL_CTL_MOD,fd,&ee)){
				break;
			}
			return 0;
		}else if(errno != EINTR){
			break;
		}
	}
	close(fd);
	return -1;
}
