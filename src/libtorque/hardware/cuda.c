#include <string.h>
#include <libtorque/internal.h>
#include <libtorque/hardware/cuda.h>

#ifndef LIBTORQUE_WITHOUT_CUDA
#include <cuda.h>
#include <cuda_runtime_api.h>
int detect_cudadevcount(void){
	int count,attr;
	CUresult cerr;

	// cuInit unknown failures (999) are typically due to a missing module
	// or a missing /dev node, especially following the introduction of
	// nvidia-uvm and /dev/nvidia-uvm. Running as root will facilitate the
	// necessary setup.
	if((cerr = cuInit(0)) != CUDA_SUCCESS){
		if(cerr == CUDA_ERROR_NO_DEVICE){
			return 0;
		}
		return -1;
	}
	if((cerr = cuDriverGetVersion(&attr)) != CUDA_SUCCESS){
		return -1;
	}
	// CUDA is backwards-, but not forwards-compatible
	if(CUDA_VERSION > attr){
		return -1;
	}
	// major = attr / 1000. minor = attr % 1000
	if((cerr = cuDeviceGetCount(&count)) != CUDA_SUCCESS){
		return -1;
	}
	return count;
}

// CUDA must already have been initialized before calling cudaid().
#define CUDASTRLEN 80
// warp/cores is more our "SIMD width" than threads per core...unused
#define CORES_PER_NVPROCESSOR 8 //  taken from CUDA 2.3 SDK's deviceQuery.cpp
torque_err cudaid(torque_cput *cpudesc,unsigned devno){
	cudaError_t cerr;
	CUresult cres;
#if CUDA_VERSION < 3020
	unsigned mem;
#else
	size_t mem;
#endif
	struct cudaDeviceProp dprop;
	CUdevice c;
	char *str;
	int attr;

	memset(cpudesc,0,sizeof(*cpudesc));
	if((cres = cuDeviceGet(&c,devno)) != CUDA_SUCCESS){
		return TORQUE_ERR_INVAL;
	}
	if((cerr = cudaGetDeviceProperties(&dprop, c)) != cudaSuccess){
		return TORQUE_ERR_INVAL;
	}
	cpudesc->spec.cuda.major = dprop.major;
	cpudesc->spec.cuda.minor = dprop.minor;
	cres = cuDeviceGetAttribute(&attr,CU_DEVICE_ATTRIBUTE_WARP_SIZE,c);
	if(cres != CUDA_SUCCESS || attr <= 0){
		return TORQUE_ERR_ASSERT;
	}
	cpudesc->threadspercore = 1;
	cres = cuDeviceGetAttribute(&attr,CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT,c);
	if(cres != CUDA_SUCCESS || attr <= 0){
		return TORQUE_ERR_ASSERT;
	}
	cpudesc->coresperpackage = attr;
	if((cres = cuDeviceTotalMem(&mem,c)) != CUDA_SUCCESS){
		return TORQUE_ERR_ASSERT;
	}
	if((str = malloc(CUDASTRLEN)) == NULL){
		return TORQUE_ERR_RESOURCE;
	}
	if((cres = cuDeviceGetName(str,CUDASTRLEN,c)) != CUDA_SUCCESS){
		free(str);
		return TORQUE_ERR_ASSERT;
	}
	cpudesc->strdescription = str;
	cpudesc->isa = TORQUE_ISA_NVIDIA;
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
