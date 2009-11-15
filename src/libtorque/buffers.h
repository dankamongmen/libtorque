#ifndef LIBTORQUE_BUFFERS
#define LIBTORQUE_BUFFERS

#ifdef __cplusplus
extern "C" {
#endif

#include <libtorque/internal.h>

// The simplest receive buffer.
static inline char *
rxbuffer_buf(libtorque_rxbuf *rxb){
	return rxb->buffer;
}

static inline size_t
rxbuffer_avail(const libtorque_rxbuf *rxb){
	return rxb->bufleft;
}

#define RXBUFSIZE (16 * 1024)

static inline int
initialize_rxbuffer(libtorque_rxbuf *rxb){
	if( (rxb->buffer = malloc(RXBUFSIZE)) ){
		rxb->bufleft = RXBUFSIZE;
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
	*valid = RXBUFSIZE - rxb->bufleft;
	return rxb->buffer;
}

#ifdef __cplusplus
}
#endif

#endif
