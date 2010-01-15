#ifndef LIBTORQUE_ALLOC
#define LIBTORQUE_ALLOC

#ifdef __cplusplus
extern "C" {
#endif

void *get_pages(size_t,int)
	__attribute__ ((warn_unused_result))
	__attribute__ ((malloc));

// Get a VMA as large as the largest (*architectural*, not necessarily
// *supported*) TLB. The presumption is that this is a nice, natural size for
// "something which might get big, but we definitely need a lot of them",
// primarily per-connection buffers. Also, such an allocation will (hopefully)
// trigger large TLB support in the OS immediately. The return is suitable for
// use with mod_pages(). Returns the actual allocation size in the parameter.
void *get_big_page(size_t *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)))
	__attribute__ ((malloc));

void *mod_pages(void *,size_t,size_t,int)
	// __attribute__ ((malloc)) cannot be used with mod_pages (if the VMA is
	// not moved, the return value will alias the input pointer).
	__attribute__ ((warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif
