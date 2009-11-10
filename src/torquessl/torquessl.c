#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <libtorque/libtorque.h>

int main(void){
	sigset_t termset;
	int sig;

	sigemptyset(&termset);
	sigaddset(&termset,SIGTERM);
	if(pthread_sigmask(SIG_SETMASK,&termset,NULL)){
		fprintf(stderr,"Couldn't set signal mask\n");
		return EXIT_FAILURE;
	}
	if(libtorque_init()){
		fprintf(stderr,"Couldn't initialize libtorque\n");
		return EXIT_FAILURE;
	}
	// FIXME add SIGTERM handler
	if(libtorque_spawn()){
		fprintf(stderr,"Couldn't spawn libtorque threads\n");
		libtorque_stop();
		return EXIT_FAILURE;
	}
	if(sigwait(&termset,&sig)){
		fprintf(stderr,"Couldn't wait on signals\n");
		libtorque_stop();
		return EXIT_FAILURE;
	}
	printf("Got signal %d (%s), closing down...\n",sig,strsignal(sig));
	if(libtorque_stop()){
		fprintf(stderr,"Couldn't shutdown libtorque\n");
		return EXIT_FAILURE;
	}
	printf("Successfully cleaned up.\n");
	return EXIT_SUCCESS;
}
