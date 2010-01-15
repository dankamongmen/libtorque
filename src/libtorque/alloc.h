#ifndef LIBTORQUE_ALLOC
#define LIBTORQUE_ALLOC

#ifdef __cplusplus
extern "C" {
#endif

void *get_pages(size_t,int)
	__attribute__ ((warn_unused_result))
	__attribute__ ((malloc));

// __attribute__ ((malloc)) cannot be used with mod_pages (if the VMA is not
// moved, the return value will alias the input pointer).
void *mod_pages(void *,size_t,size_t,int)
	__attribute__ ((warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif
