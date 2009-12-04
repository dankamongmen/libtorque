#include <libtorque/buffers.h>

int buffered_rxfxn(int fd,libtorque_cbctx *cbctx,void *cbstate){
	return printf("%d %p %p\n",fd,cbctx,cbstate);
}
