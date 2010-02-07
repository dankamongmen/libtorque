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
#define CUDASTRLEN 80
torque_err cudaid(torque_cput *cpudesc,unsigned devno){
	CUresult cerr;
	unsigned mem;
	CUdevprop p;
	CUdevice c;
	char *str;

	memset(cpudesc,0,sizeof(*cpudesc));
	if((cerr = cuDeviceGet(&c,devno)) != CUDA_SUCCESS){
		return TORQUE_ERR_INVAL;
	}
	if((cerr = cuDeviceGetProperties(&p,c)) != CUDA_SUCCESS){
		return TORQUE_ERR_ASSERT;
	}
	if((cerr = cuDeviceTotalMem(&mem,c)) != CUDA_SUCCESS){
		return TORQUE_ERR_ASSERT;
	}
	// FIXME do something with mem -- new NUMA node?
	if((str = malloc(CUDASTRLEN)) == NULL){
		return TORQUE_ERR_RESOURCE;
	}
	if((cerr = cuDeviceGetName(str,CUDASTRLEN,c)) != CUDA_SUCCESS){
		free(str);
		return TORQUE_ERR_ASSERT;
	}
	cpudesc->strdescription = str;
	return 0;
}
#undef CUDASTRLEN
#else
int detect_cudadevcount(void){
	return 0;
}

torque_err cudaid(torque_cput *cpudesc __attribute__ ((unused)),
			unsigned devno __attribute__ ((unused))){
	return TORQUE_ERR_UNAVAIL;
}
#endif
