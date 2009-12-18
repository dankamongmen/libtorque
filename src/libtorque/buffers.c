#include <errno.h>
#include <unistd.h>
#include <libtorque/buffers.h>
#include <libtorque/libtorque.h>

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
			// FIXME if no space was cleared, grow that fucker
		}
		if((r = read(fd,rxb->buffer + rxb->bufate,rxb->buftot - rxb->bufoff)) > 0){
			rxb->bufoff += r;
		}else if(r == 0){
			if(callback(rxb,fd,cbctx,cbstate)){
				break;
			}
			return 0; // FIXME
		}else if(errno == EAGAIN){
			if(callback(rxb,fd,cbctx,cbstate)){
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
