#include <stdlib.h>
#include <sys/socket.h>
#include <libtorque/conn.h>

torque_conncb *create_conncb(void *rx,void *tx,void *state){
	torque_conncb *ret;

	if( (ret =  malloc(sizeof(*ret))) ){
		ret->cbstate = state;
		ret->txfxn = tx;
		ret->rxfxn = rx;
	}
	return ret;
}

void conn_unbuffered_txfxn(int fd,void *state){
	torque_conncb *cbctx = state;

	if(connect(fd,NULL,0) == 0){
		// FIXME substitute child state
		// FIXME set up new events of interest based off rx/tx
		free_conncb(cbctx);
	}else{
		// FIXME get error back to user
	}
}

void free_conncb(torque_conncb *cb){
	if(cb){
		free(cb);
	}
}
