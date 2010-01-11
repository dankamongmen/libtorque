STATDEF(rounds)		// times through the event queue loop
STATDEF(events)		// events dequeued from kernel
STATDEF(errors)		// errors in the main event code
STATDEF(utimeus)	// microseconds of user time
STATDEF(stimeus)	// microseconds of system time
STATDEF(vctxsw)		// voluntary context switches
STATDEF(ictxsw)		// involuntary context switches
PTRDEF(stackptr)	// stack base pointer
STATDEF(stacksize)	// stack size in bytes

// Events we track, especially errors
STATDEF(pollerr)	// errors in the core event retrieval call
