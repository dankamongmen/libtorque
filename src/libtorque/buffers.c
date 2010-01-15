#include <errno.h>
#include <unistd.h>
#include <libtorque/buffers.h>
#include <libtorque/libtorque.h>
#include <libtorque/events/thread.h>

static inline int
callback(libtorque_rxbuf *rxb,int fd,libtorque_cbctx *cbctx,void *cbstate){
	if(rxb->bufoff - rxb->bufate){
		return ((const libtorquebrcb)cbctx->cbstate)(fd,cbctx,cbstate);
	}
	return 0;
}

static int
restorefd(int fd,int eflags){
	struct epoll_event ee;
	evhandler *evh;

	// eflags should only be 0 or EPOLLOUT
	evh = get_thread_evh();
	memset(&ee,0,sizeof(ee));
	// EPOLLRDHUP isn't available prior to kernel 2.6.17 and GNU libc 2.6.
	// We shouldn't need it, though.
	ee.events = EPOLLIN | EPOLLET | EPOLLONESHOT | eflags;
	ee.data.fd = fd;
	if(epoll_ctl(evh->evq->efd,EPOLL_CTL_MOD,fd,&ee)){
		return -1;
	}
	return 0;
}

static int
growrxbuf(libtorque_rxbuf *rxb){
	typeof(*rxb->buffer) *tmp;
	size_t news;

	news = rxb->buftot + RXBUFSIZE; // FIXME blehhhh
	if((tmp = realloc(rxb->buffer,news)) == NULL){
		return -1;
	}
	rxb->buffer = tmp;
	rxb->buftot = news;
	return 0;
}

void buffered_rxfxn(int fd,libtorque_cbctx *cbctx,void *cbstate){
	libtorque_rxbuf *rxb = cbctx->rxbuf;
	int r;

	for( ; ; ){
		if(rxb->buftot - rxb->bufoff == 0){
			int cb;

			if((cb = callback(rxb,fd,cbctx,cbstate)) < 0){
				break;
			}
			if(rxb->buftot - rxb->bufoff == 0){
				if(growrxbuf(rxb)){
					break;
				}
			}
		}
		if((r = read(fd,rxb->buffer + rxb->bufoff,rxb->buftot - rxb->bufoff)) > 0){
			rxb->bufoff += r;
		}else if(r == 0){
			int cb;

			// must close, *unless* TX indicated
			if((cb = callback(rxb,fd,cbctx,cbstate)) <= 0){
				break;
			}
			if(restorefd(fd,EPOLLOUT)){
				break;
			}
			return;
		}else if(errno == EAGAIN || errno == EWOULDBLOCK){
			int cb;

			if((cb = callback(rxb,fd,cbctx,cbstate)) < 0){
				break;
			}
			if(restorefd(fd,cb ? EPOLLOUT : 0)){
				break;
			}
			return;
		}else if(errno != EINTR){
			break;
		}
	}
	close(fd);
}
