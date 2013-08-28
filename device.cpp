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
}

device::~device(void) {
	buffers_.DestroyAll();
} 

void device::Init(const cl_device_id &single_device) {
	id_ = single_device;
}

result device::KernelInit(const cl_program &program, const size_t &kernel_count, const string *kernels) {
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

cl_kernel device::kernel(const string &kernel) {
	return kernel_[kernel];
}

cl_kernel device::NewKernelInstance(const string &kernel) {
	return clCreateKernel(program_, kernel.c_str(), NULL);
}

cl_command_queue device::cq() {

	cl_command_queue new_cq = clCreateCommandQueue(g_context, 
												   id_, 
												   CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
												   NULL);

	return new_cq;
}

