#include <errno.h>
#include <unistd.h>
#include <libtorque/buffers.h>
#include <libtorque/libtorque.h>

static inline int
callback(libtorque_rxbuf *rxb,int fd,libtorque_cbctx *cbctx,void *cbstate){
	if(rxb->bufoff - rxb->bufate){
		printf("cb: %ju\n",(uintmax_t)rxb->bufoff - rxb->bufate);
		return ((const libtorquercb)cbctx->cbstate)(fd,cbctx,cbstate);
	}
	printf("nocb\n");
	return 0;
}

int buffered_rxfxn(int fd,libtorque_cbctx *cbctx,void *cbstate){
	libtorque_rxbuf *rxb = cbctx->rxbuf;
	int r;

	do{
		if((r = read(fd,rxb->buffer + rxb->bufate,rxb->buftot - rxb->bufoff)) > 0){
			rxb->bufoff += r;
		}else if(r == 0){
			printf("cbstate: %p\n",cbctx->cbstate);
			// FIXME final callback, if there's any data
			if(callback(rxb,fd,cbctx,cbstate) == 0){
				close(fd);
			}
			return 0;
		}else if(errno == EAGAIN){
			return callback(rxb,fd,cbctx,cbstate);
		}
		printf("read %d from %d\n",r,fd);
	}while(r > 0 || errno == EINTR);
	printf("error (%s) on %d\n",strerror(errno),fd);
	close(fd);
	return -1;
}
