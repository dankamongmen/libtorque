#ifndef LIBTORQUE_BUFFERS
#define LIBTORQUE_BUFFERS

#ifdef __cplusplus
extern "C" {
#endif

#include <libtorque/internal.h>

// The simplest receive buffer.
static inline void
rxbuffer_advance(libtorque_rxbuf *rxb,size_t s){
	rxb->bufate += s;
}

#define RXBUFSIZE (16 * 1024)

static inline int
initialize_rxbuffer(libtorque_rxbuf *rxb){
	if( (rxb->buffer = malloc(RXBUFSIZE)) ){
		rxb->bufoff = rxb->bufate = 0;
		rxb->buftot = RXBUFSIZE;
		return 0;
	}
	return -1;
}

static inline void
free_rxbuffer(libtorque_rxbuf *rxb){
	free(rxb->buffer);
}

static inline const char *
rxbuffer_valid(const libtorque_rxbuf *rxb,size_t *valid){
	*valid = rxb->bufoff - rxb->bufate;
	return rxb->buffer + rxb->bufate;
}

#ifndef LIBTORQUE_WITHOUT_SSL
#include <openssl/ssl.h>
static inline int
rxbuffer_ssl(libtorque_rxbuf *rxb,SSL *s){
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