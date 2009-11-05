#ifndef LIBTORQUE_LIBTORQUE
#define LIBTORQUE_LIBTORQUE

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the library, returning 0 on success. No libtorque functions may
// be called before a successful call to libtorque_init(). libtorque_init() may
// not be called again until libtorque_stop() has been called. Implicitly, only
// one thread may call libtorque_init().
int libtorque_init(void) __attribute__ ((visibility("default")));

// Reset the library, destroying all associated threads and state and returning
// 0 on success. No libtorque functions, save libtorque_init(), may be called
// following a call to libtorque_stop() (whether it is successful or not).
// libtorque_init() must not be called from multiple threads, or while another
// thread is calling a libtorque function. A successful return guarantees that
// no further callbacks will be issued.
int libtorque_stop(void) __attribute__ ((visibility("default")));

#ifdef __cplusplus
}
#endif

#endif
