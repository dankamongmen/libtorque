#ifndef torque_BUFFERS
#define torque_BUFFERS

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>
#include <libtorque/alloc.h>
#include <libtorque/internal.h>

// This is the simplest possible RX buffer; fixed-length, one piece, not even
// circular (ie, fixed length on connection!). It'll be replaced.
typedef struct torque_rxbuf {
	char *buffer;			// always points to the buffer's start
	size_t buftot;			// length of the buffer
	size_t bufoff;			// how far we've dirtied the buffer
	size_t bufate;			// how much input the client's released
	libtorquebrcb rx;		// inner rx callback
	libtorquebwcb tx;		// inner tx callback
} torque_rxbuf;

typedef struct torque_rxbufcb {
	torque_rxbuf rxbuf;
	void *cbstate;			// userspace callback
} torque_rxbufcb;

// The simplest receive buffer.
static inline void
rxbuffer_advance(torque_rxbuf *rxb,size_t s){
	if((rxb->bufate += s) == rxb->buftot){
		rxb->bufoff = rxb->bufate = 0;
	}else if(rxb->bufoff == rxb->buftot){
		memmove(rxb->buffer,rxb->buffer + rxb->bufate,
				rxb->bufoff - rxb->bufate);
		rxb->bufoff -= rxb->bufate;
		rxb->bufate = 0;
	} // FIXME very slow, doesn't reclaim memory, sucky in general
}

static inline int initialize_rxbuffer(const struct torque_ctx *,torque_rxbuf *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,2)));

static inline int
initialize_rxbuffer(const struct torque_ctx *ctx,torque_rxbuf *rxb){
	if( (rxb->buffer = get_big_page(ctx,&rxb->buftot)) ){
		rxb->bufoff = rxb->bufate = 0;
		return 0;
	}
	return -1;
}

static inline torque_rxbufcb *create_rxbuffercb(struct torque_ctx *,
				libtorquebrcb,libtorquebwcb,void *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)))
	__attribute__ ((malloc));

static inline torque_rxbufcb *
create_rxbuffercb(struct torque_ctx *ctx,libtorquebrcb rx,libtorquebwcb tx,
					void *cbstate){
	torque_rxbufcb *ret;

	if( (ret = malloc(sizeof(*ret))) ){
		if(initialize_rxbuffer(ctx,&ret->rxbuf) == 0){
			ret->rxbuf.rx = rx;
			ret->rxbuf.tx = tx;
			ret->cbstate = cbstate;
			return ret;
		}
		free(ret);
		ret = NULL;
	}
	return ret;
}

static inline void
free_rxbuffer(torque_rxbuf *rxb){
	dealloc(rxb->buffer,rxb->buftot);
}

static inline void
free_rxbuffercb(torque_rxbufcb *rxb){
	free_rxbuffer(&rxb->rxbuf);
}

static inline const char *
rxbuffer_valid(const torque_rxbuf *rxb,size_t *valid){
	*valid = rxb->bufoff - rxb->bufate;
	return rxb->buffer + rxb->bufate;
}

void buffered_txfxn(int,void *) __attribute__ ((nonnull(2)));
void buffered_rxfxn(int,void *) __attribute__ ((nonnull(2)));

#ifndef torque_WITHOUT_SSL
#include <openssl/ssl.h>
static inline int
rxbuffer_ssl(torque_rxbuf *rxb,SSL *s){
	int r;

	if((r = SSL_read(s,rxb->buffer + rxb->bufate,rxb->buftot - rxb->bufoff)) > 0){
		rxb->bufoff += r;
	}
	return r;
}
#endif

#ifdef __cplusplus
}
#endif

#endif
