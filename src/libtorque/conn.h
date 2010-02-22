#ifndef TORQUE_CONN
#define TORQUE_CONN

typedef struct torque_conncb {
	void *rxfxn;
	void *txfxn;
	void *cbstate;			// userspace callback state
} torque_conncb;

torque_conncb *create_conncb(void *,void *,void *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((malloc));

void conn_unbuffered_txfxn(int,void *);

struct torque_rxbuf;

int conn_txfxn(int,struct torque_rxbuf *,void *)
	__attribute__ ((warn_unused_result));

void free_conncb(torque_conncb *);

#endif
