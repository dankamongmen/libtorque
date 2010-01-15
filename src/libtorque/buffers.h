#ifndef LIBTORQUE_BUFFERS
#define LIBTORQUE_BUFFERS

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>
#include <libtorque/alloc.h>
#include <libtorque/internal.h>

// The simplest receive buffer.
static inline void
rxbuffer_advance(libtorque_rxbuf *rxb,size_t s){
	if((rxb->bufate += s) == rxb->buftot){
		rxb->bufoff = rxb->bufate = 0;
	}else if(rxb->bufoff == rxb->buftot){
		memmove(rxb->buffer,rxb->buffer + rxb->bufate,
				rxb->bufoff - rxb->bufate);
		rxb->bufoff -= rxb->bufate;
		rxb->bufate = 0;
	} // FIXME very slow, doesn't reclaim memory, sucky in general
}

#define RXBUFSIZE (16 * 1024) // FIXME embarrassing

static inline int initialize_rxbuffer(libtorque_rxbuf *)
	__attribute__ ((warn_unused_result));

static inline int
initialize_rxbuffer(libtorque_rxbuf *rxb){
	if( (rxb->buffer = get_big_page(&rxb->buftot)) ){
		rxb->bufoff = rxb->bufate = 0;
		return 0;
	}
	return -1;
}

static inline libtorque_rxbuf *create_rxbuffer(void)
	__attribute__ ((warn_unused_result))
	__attribute__ ((malloc));

static inline libtorque_rxbuf *
create_rxbuffer(void){
	libtorque_rxbuf *ret;

	if( (ret = malloc(sizeof(*ret))) ){
		if(initialize_rxbuffer(ret)){
			free(ret);
			ret = NULL;
		}
	}
	return ret;
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

void buffered_rxfxn(int,libtorque_cbctx *,void *)
	__attribute__ ((nonnull(2)));

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
