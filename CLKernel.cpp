/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.04
 *
 * Copyright 2013, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#include "CLKernel.h"
#include "CLutil.h"
#include "device.h"


CLKernel::CLKernel(const int &device_id, const string &kernel_name) {
	kernel_				= g_devices[device_id].NewKernelInstance(kernel_name);
	arguments_valid_	= true;
	argument_counter_	= 0;
	work_dim_			= 0;			
	local_work_size_	= NULL;	
	scalar_global_size_	= NULL;
	scalar_item_size_	= NULL;	
}

void CLKernel::SetArg(const size_t &arg_size, const void *arg_ptr) {
	if (arguments_valid_) {
		cl_int cl_status = clSetKernelArg(kernel_, argument_counter_++, arg_size, arg_ptr);
		if (cl_status != CL_SUCCESS) {
			g_last_cl_error = cl_status;
			arguments_valid_ = false;
		}
	}
}

void CLKernel::SetNumberedArg(const int &arg_number, const size_t &arg_size, const void *arg_ptr) {
	if (arguments_valid_) {
		cl_int cl_status = clSetKernelArg(kernel_, arg_number, arg_size, arg_ptr);
		if (cl_status != CL_SUCCESS) {
			g_last_cl_error = cl_status;
			arguments_valid_ = false;
		}
	}
}

bool CLKernel::arguments_valid(){
	return arguments_valid_;
}

void CLKernel::set_work_dim(const cl_uint &dimensions) {
	if (dimensions > 0 && dimensions < 4) {
		work_dim_			= dimensions;
		local_work_size_	= new size_t [dimensions];
		scalar_global_size_ = new size_t [dimensions];
		scalar_item_size_	= new size_t [dimensions];
		global_work_size_	= new size_t [dimensions];

		for (cl_uint i = 0; i < work_dim_; ++i) {
			local_work_size_[i]		= 0;
			scalar_global_size_[i]	= 0;
			scalar_item_size_[i]	= 0;
		}
	}
}

void CLKernel::set_local_work_size(const size_t *size) {
	for (cl_uint i = 0; i < work_dim_; ++i) {
		local_work_size_[i] = size[i];
	}
}

void CLKernel::set_scalar_global_size(const size_t *size) {
	for (cl_uint i = 0; i < work_dim_; ++i) {
		scalar_global_size_[i] = size[i];
	}
}

void CLKernel::set_scalar_item_size(const size_t *size) {
	for (cl_uint i = 0; i < work_dim_; ++i) {
		scalar_item_size_[i] = size[i];
	}
}

void CLKernel::global_work_size() {
	// Global execution domain is computed from specified
	// scalar dimensions.
	// In OpenCL global_work_size must be an integer multiple
	// of set_local_work_size.

	for (cl_uint i = 0; i < work_dim_; ++i) {
		const float items_per_global = static_cast<float>(scalar_global_size_[i])
									 / static_cast<float>(scalar_item_size_[i]);
		const float global_per_local = items_per_global / static_cast<float>(local_work_size_[i]);
		const size_t rounded_items	 = static_cast<size_t>(ceil(global_per_local));
		global_work_size_[i]		 = rounded_items * local_work_size_[i];
	}
}

result CLKernel::Execute(
	const	cl_command_queue	&cq, 
			cl_event			*event) {

	if (work_dim_ > 0 && work_dim_ < 4) {
		global_work_size();
		cl_int cl_status = clEnqueueNDRangeKernel(cq, 
												  kernel_,
												  work_dim_,
												  NULL,
												  global_work_size_,
												  local_work_size_,
												  0,
												  NULL,
												  event);
		if (cl_status != CL_SUCCESS) {
			g_last_cl_error = cl_status;
			*event = NULL;
			return FILTER_ERROR;
		}

		return FILTER_OK;
	}
	return FILTER_ERROR;
}

result CLKernel::ExecuteAsynch(
	const	cl_command_queue	&cq, 
			cl_event			*antecedent, 
			cl_event			*event) {

	if (work_dim_ > 0 && work_dim_ < 4) {
		global_work_size();
		cl_int cl_status = clEnqueueNDRangeKernel(cq, 
												  kernel_,
												  work_dim_,
												  NULL,
												  global_work_size_,
												  local_work_size_,
												  1,
												  antecedent,
												  event);
		if (cl_status != CL_SUCCESS) {
			g_last_cl_error = cl_status;
			*event = NULL;
			return FILTER_ERROR;
		}

		return FILTER_OK;
	}
	return FILTER_ERROR;
}
result CLKernel::ExecuteWaitList(
	const	cl_command_queue	&cq, 
	const	int					&WaitListLength, 
			cl_event			*antecedents, 
			cl_event			*event) {

	if (work_dim_ > 0 && work_dim_ < 4) {
		global_work_size();
		cl_int cl_status = clEnqueueNDRangeKernel(cq, 
												  kernel_,
												  work_dim_,
												  NULL,
												  global_work_size_,
												  local_work_size_,
												  WaitListLength,
												  antecedents,
												  event);
		if (cl_status != CL_SUCCESS) {
			g_last_cl_error = cl_status;
			*event = NULL;
			return FILTER_ERROR;
		}

		return FILTER_OK;
	}
	return FILTER_ERROR;
}