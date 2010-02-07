#include <libtorque/internal.h>
#include <libtorque/hardware/cuda.h>

#ifndef LIBTORQUE_WITHOUT_CUDA
#include <cuda/cuda.h>
int detect_cudadevcount(void){
	CUresult cerr;
	int count;

	if((cerr = cuInit(0)) != CUDA_SUCCESS){
		if(cerr == CUDA_ERROR_NO_DEVICE){
			return 0;
		}
		return -1;
	}
	if((cerr = cuDeviceGetCount(&count)) != CUDA_SUCCESS){
		return -1;
	}
	return count;
}

// CUDA must already have been initialized before calling cudaid().
torque_err cudaid(torque_cput *cpudesc,unsigned devno){
	CUresult cerr;
	CUdevice c;

	if((cerr = cuDeviceGet(&c,devno)) != CUDA_SUCCESS){
		return TORQUE_ERR_INVAL;
	}
	// FIXME work it
	if(cpudesc == NULL){
		return -1;
	}
	return 0;
}
#else
int detect_cudadevcount(void){
	return 0;
}

torque_err cudaid(torque_cput *cpudesc __attribute__ ((unused)),
			unsigned devno __attribute__ ((unused))){
	return TORQUE_ERR_UNAVAIL;
}
#endif
