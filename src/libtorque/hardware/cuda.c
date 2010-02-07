#include <cuda/cuda.h>
#include <libtorque/internal.h>
#include <libtorque/hardware/cuda.h>

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
