#ifndef LIBTORQUE_PORT_GCC
#define LIBTORQUE_PORT_GCC

#include <features.h>

#ifdef __GNUC_PREREQ
# if __GNUC_PREREQ (2, 5)
#  define CONSTFUNC __attribute__ ((const))
#  if __GNUC_PREREQ (2, 95)
#   define LIKELY(expr) __builtin_expect((expr),1)
#   define UNLIKELY(expr) __builtin_expect((expr),0)
#   if __GNUC_PREREQ (2, 96)
#    define PUREFUNC __attribute__ ((pure))
#    if __GNUC_PREREQ (3, 3)
#     define NOTHROW __attribute__ ((__nothrow__))
#     if __GNUC_PREREQ (4, 3)
#      define ALLOC_SIZE(z) __attribute__ ((alloc_size(z)))
#     else
#      define ALLOC_SIZE(z)
#     endif
#    else
#     define NOTHROW
#    endif
#   else
#    define PUREFUNC
#   endif
#  else
#   define LIKELY(expr) (expr)
#   define UNLIKELY(expr) (expr)
#   warning "__builtin_expect not supported on this GCC"
#  endif
# else
#  define CONSTFUNC
# endif
#else
# define CONSTFUNC
# define PUREFUNC
# define NOTHROW
# define ALLOCSIZE(x)
# define LIKELY(expr) (expr)
# define UNLIKELY(expr) (expr)
# warning "__builtin_expect not supported on this compiler"
#endif

#endif
