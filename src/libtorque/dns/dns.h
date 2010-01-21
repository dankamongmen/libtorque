#ifndef LIBTORQUE_DNS_DNS
#define LIBTORQUE_DNS_DNS

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LIBTORQUE_WITHOUT_ADNS
#include <adns.h>
typedef adns_state dns_state;
#else
typedef struct {
	int fd;
} dns_state;
#endif

struct evqueue;
struct libtorque_ctx;

int libtorque_dns_init(dns_state *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

int load_dns_fds(struct libtorque_ctx *,dns_state *,const struct evqueue *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1,2,3)));

void libtorque_dns_shutdown(dns_state *);

#ifdef __cplusplus
}
#endif

#endif
