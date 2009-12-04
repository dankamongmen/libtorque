#include <errno.h>
#include <unistd.h>
#include <libtorque/buffers.h>

int buffered_rxfxn(int fd,libtorque_cbctx *cbctx,void *cbstate){
	libtorque_rxbuf *rxb = cbctx->rxbuf;
	int r;

	do{
		if((r = read(fd,rxb->buffer + rxb->bufate,rxb->buftot - rxb->bufoff)) > 0){
			rxb->bufoff += r;
		}else if(r == 0){
			printf("cbstate: %p\n",cbctx->cbstate);
			// FIXME final callback, if there's any data
			close(fd);
			return 0;
		}else if(errno == EAGAIN){
			// callback if there's data
			return 0;
		}
		printf("read %d for %p from %d\n",r,cbstate,fd);
	}while(r > 0 || errno == EINTR);
	printf("error (%s) on %d\n",strerror(errno),fd);
	close(fd);
	return -1;
}
