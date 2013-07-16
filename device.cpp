/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.00
 *
 * Copyright 2011, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#include "device.h"
#include "CLutil.h"
#include "result.h"


device::device() {
	id_ = NULL;
	cq_ = NULL;
	ready_ = false;
}

device::~device(void) {
	if (ready_)	DeleteCommandQueue();
} 

result device::Init(const cl_device_id &single_device) {
	id_ = single_device;

	cl_int status;

	cq_ = clCreateCommandQueue(g_context, 
							   id_, 
							   CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
							   &status);

	if (status == CL_SUCCESS) { 
		ready_ = true;
		return FILTER_OK;
	} else {
		ready_ = false;
		return FILTER_ERROR;
	}
}

result device::DeleteCommandQueue() {
	// The command queue can't actually be deleted. It can only have its
	// reference count reduced, and it will delete itself when it 
	// sees that its reference count hits zero.
	// So return an error if cq_ exists after release.
	if (ready_)	{
		buffers_.DestroyAll();
		clReleaseCommandQueue(cq_);
	}

	if (cq_ == NULL) { // TODO check if it's correct that cq_ is set to NULL when it is released
		ready_ = false;
		return FILTER_OK;
	} else {
		ready_ = true;
		return FILTER_ERROR;
	}
}

result device::KernelInit(const cl_program &program, const size_t &kernel_count, const string* kernels) {
	cl_int status = CL_SUCCESS;

	program_ = program;

	for (size_t i = 0; i < kernel_count; ++i) {
		kernel_[kernels[i]] = clCreateKernel(program, kernels[i].c_str(), &status);
		if (status != CL_SUCCESS) { 
			g_last_cl_error = status;
			return FILTER_OPENCL_KERNEL_INITIALISATION_FAILED;
		}
	}

	return FILTER_OK;
}

cl_kernel device::kernel(const string& kernel) {
	return kernel_[kernel];
}

cl_kernel device::NewKernelInstance(const string& kernel) {
	return clCreateKernel(program_, kernel.c_str(), NULL);
}

cl_command_queue device::cq() {

	cl_command_queue new_cq = clCreateCommandQueue(g_context, 
						   id_, 
						   CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
						   NULL);

	return new_cq;
}

bool device::ready() {
	return ready_; // TODO consider querying cq_ != NULL instead
}


