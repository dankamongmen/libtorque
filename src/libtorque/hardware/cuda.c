#include <libtorque/internal.h>
#include <libtorque/hardware/cuda.h>

#ifndef LIBTORQUE_WITHOUT_CUDA
#include <cuda.h>
int detect_cudadevcount(void){
	int count,attr;
	CUresult cerr;

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
	CUresult cerr;
#if CUDA_VERSION < 4000
	unsigned mem;
#else
	size_t mem;
#endif
	CUdevice c;
	char *str;
	int attr;

	memset(cpudesc,0,sizeof(*cpudesc));
	if((cerr = cuDeviceGet(&c,devno)) != CUDA_SUCCESS){
		return TORQUE_ERR_INVAL;
	}
	cerr = cuDeviceGetAttribute(&attr,CU_DEVICE_ATTRIBUTE_WARP_SIZE,c);
	if(cerr != CUDA_SUCCESS || attr <= 0){
		return TORQUE_ERR_ASSERT;
	}
	cpudesc->threadspercore = 1;
	cerr = cuDeviceGetAttribute(&attr,CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT,c);
	if(cerr != CUDA_SUCCESS || attr <= 0){
		return TORQUE_ERR_ASSERT;
	}
	cpudesc->coresperpackage = attr;
	if((cerr = cuDeviceTotalMem(&mem,c)) != CUDA_SUCCESS){
		return TORQUE_ERR_ASSERT;
	}
	// FIXME do something with mem -- new NUMA node?
	if((cerr = cuDeviceComputeCapability(&cpudesc->spec.cuda.major,
			&cpudesc->spec.cuda.minor,c)) != CUDA_SUCCESS){
		return TORQUE_ERR_ASSERT;
	}
	if((str = malloc(CUDASTRLEN)) == NULL){
		return TORQUE_ERR_RESOURCE;
	}
	if((cerr = cuDeviceGetName(str,CUDASTRLEN,c)) != CUDA_SUCCESS){
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
