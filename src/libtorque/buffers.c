#include <errno.h>
#include <unistd.h>
#include <libtorque/buffers.h>
#include <libtorque/events/thread.h>

static inline int
rxback(torque_rxbuf *rxb,int fd,void *cbstate){
	if(rxb->bufoff - rxb->bufate){
		return rxb->rx(fd,rxb,cbstate);
	}
	return 0;
}

static inline int
txback(torque_rxbuf *rxb,int fd,void *cbstate){
	if(rxb->bufoff - rxb->bufate){
		return rxb->tx(fd,rxb,cbstate);
	}
	return 0;
}

static int
growrxbuf(torque_rxbuf *rxb){
	typeof(*rxb->buffer) *tmp;
	size_t news;

	news = rxb->buftot * 2; // FIXME hrmmm
	if((tmp = mod_pages(rxb->buffer,rxb->buftot,news)) == NULL){
		return -1;
	}
	rxb->buffer = tmp;
	rxb->buftot = news;
	return 0;
}

void buffered_txfxn(int fd,void *cbstate){
	torque_rxbufcb *cbctx = cbstate;
	torque_rxbuf *rxb = &cbctx->rxbuf;
	int cb;

	// FIXME very likely incomplete
	if((cb = txback(rxb,fd,cbstate)) < 0){
		goto err;
	}
	if(restorefd(get_thread_evh(),fd,EVREAD)){
		goto err;
	}
	return;

err:
	close(fd);
}

void buffered_rxfxn(int fd,void *cbstate){
	torque_rxbufcb *cbctx = cbstate;
	torque_rxbuf *rxb = &cbctx->rxbuf;
	int r;

	for( ; ; ){
		if(rxb->buftot - rxb->bufoff == 0){
			int cb;

			if( (cb = rxback(rxb,fd,cbstate)) ){
				return;
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

			if( (cb = rxback(rxb,fd,cbstate)) ){
				return;
			}
			if(restorefd(get_thread_evh(),fd,EVWRITE)){
				break;
			}
			return;
		}else if(errno == EAGAIN || errno == EWOULDBLOCK){
			int cb;

			if( (cb = rxback(rxb,fd,cbstate)) ){
				return;
			}
			// FIXME sometimes we'll need EVWRITE as well!
			if(restorefd(get_thread_evh(),fd,EVREAD)){
				break;
			}
			return;
		}else if(errno != EINTR){
			break;
		}
	}
	// On any internal error, we're responsible for closing the fd.
	close(fd);
}
