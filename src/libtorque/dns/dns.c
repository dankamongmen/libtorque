#include <libtorque/dns/dns.h>

int libtorque_dns_init(dns_state *dctx){
#ifndef LIBTORQUE_WITHOUT_ADNS
	adns_initflags flags = adns_if_debug;

	if(adns_init(dctx,flags,NULL)){
		return -1;
	}
#else
	memset(dctx,0,sizeof(*dctx));
#endif
	return 0;
}

void libtorque_dns_shutdown(dns_state *dctx __attribute__ ((unused))){
}
