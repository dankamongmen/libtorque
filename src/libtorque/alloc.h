#ifndef LIBTORQUE_ALLOC
#define LIBTORQUE_ALLOC

#include <libtorque/port/gcc.h>

#ifdef __cplusplus
extern "C" {
#endif

struct torque_ctx;

// All of these functions return NULL on failure (not MAP_FAILED as might be
// expected). Similarly, NULL is unacceptable as input to mod_pages(), etc.

void *get_pages(size_t)
	__attribute__ ((warn_unused_result))
	__attribute__ ((malloc));

// Aligned to the smallest cacheline length. The alignment factor used is
// stored into the last parameter, if non-NULL. Good for:
//
//  - frequently-accessed, unshared data (to avoid false sharing)
//  - small truly shared data, such as synchronization variables (to avoid
//     truly shared line splits)
void *hwaligned_alloc_tight(size_t,size_t *)
	__attribute__ ((warn_unused_result))
	ALLOC_SIZE(1)
	__attribute__ ((malloc));

// Aligned to the least common multiple of hardware cacheline lengths
// (hopefully, this is simply the largest cacheline length). The alignment
// factor used is stored into the last parameter, if non-NULL. Good for:
//
//  - bulk data truly shared among multiple threads
//  - buffers and arenas whose sizes are functions of large caches' size
//  - data which is randomly accessed in ~cacheline-sized records (an arena
//     allocator ought request a block, and then parcel it out)
void *hwaligned_alloc_tight(size_t,size_t *)
	__attribute__ ((warn_unused_result))
	ALLOC_SIZE(1)
	__attribute__ ((malloc));

// Get a VMA as large as the largest (*architectural*, not necessarily
// *supported*) TLB. The presumption is that this is a nice, natural size for
// "something which might get big, but we definitely need a lot of them",
// primarily per-connection buffers. Also, such an allocation will (hopefully)
// trigger large TLB support in the OS immediately. The return is suitable for
// use with mod_pages(). Returns the actual allocation size in the parameter.
void *get_big_page(const struct torque_ctx *,size_t *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)))
	__attribute__ ((malloc));

void *get_stack(size_t *)
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)))
	__attribute__ ((malloc));

void *mod_pages(void *,size_t,size_t)
	// __attribute__ ((malloc)) cannot be used with mod_pages (if the VMA
	// is not moved, the return value will alias the input pointer).
	__attribute__ ((warn_unused_result))
	__attribute__ ((nonnull(1)));

void dealloc(void *,size_t)
	__attribute__ ((nonnull(1)));

#ifdef __cplusplus
}
#endif

#endif
