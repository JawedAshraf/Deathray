/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.04
 *
 * Copyright 2013, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#ifndef _CLKERNEL_H_
#define _CLKERNEL_H_

#include <CL/cl.h>
#include <string>
#include "result.h"

using namespace std;

// CLKernel
// Manages a kernel from creation to execution. 
class CLKernel {
public:
	CLKernel() {}

	// Constructor
	// Specify the device used to execute the named kernel.
	CLKernel(const int &device_id, const string &kernel_name);

	~CLKernel() {}

	// SetArg
	// Set the next argument in the sequence of kernel arguments.
	// Since this generates implicit argument numbers, the call ordering
	// must correspond with the kernel's argument ordering
	// Caller should check g_last_cl_error if reason for failure is required
	void SetArg(const size_t &arg_size, const void *arg_ptr);
	
	// SetNumberedArg
	// Set, or change, a numbered argument.
	// Argument numbering is zero-based.
	// Caller should check g_last_cl_error if reason for failure is required
	void SetNumberedArg(const int &arg_number, const size_t &arg_size, const void *arg_ptr);
	
	// arguments_valid
	// If an error occurred during argument setup, this will be false.
	// There is no way for caller to make this true, i.e. once an
	// argument has generated a problem, this kernel object is no longer usable
	// to execute a kernel.
	bool arguments_valid();
		
	// set_work_dim
	// Count of dimensions: 1, 2 or 3.
	void set_work_dim(const cl_uint &dimensions);
	
	// set_local_work_size
	// Layout of work items that make up a single work group. If the kernel has 
	// required dimensions specified as an attribute, then this must match. 
	// Count of elements in this array must match the dimensions set by set_work_dim().
	void set_local_work_size(const size_t *size);
	
	// set_scalar_global_size
	// Size of the global execution domain expressed as scalars. This is the basis
	// of computation of global_work_size_ for kernel execution.
	// e.g. for a frame result of NxM floats this would be {N,M}.
	// Count of elements in this array must match the dimensions set by set_work_dim().
	void set_scalar_global_size(const size_t *size);
	
	// set_scalar_item_size
	// Size of each work item in terms of scalars. Coupled with set_scalar_global_size 
	// and set_local_work_size the global_work_size_ for execution of the kernel is 
	// computed.
	// This must correspond with the layout of scalars computed per work item.
	// Count of elements in this array must match the dimensions set by set_work_dim().
	void set_scalar_item_size(const size_t *size);
	
	// Execute
	// Kernel is executed. Arguments are checked as being valid and execution 
	// parameters are computed. Execution is not complete when returning.
	//
	// Event is set to NULL if execution fails, otherwise the event can be
	// used to track execution completion.
	result Execute(
		const	cl_command_queue	&cq, 
				cl_event			*event);
	
	// ExecuteAsynch
	// Kernel is executed. Arguments are checked as being valid and execution 
	// parameters are computed. Execution is not complete when returning.
	//
	// Execution cannot proceed until the event antecedent has completed.
	//
	// Event is set to NULL if execution fails, otherwise the event can be
	// used to track execution completion.
	result ExecuteAsynch(
		const	cl_command_queue	&cq, 
				cl_event			*antecedent, 
				cl_event			*event);
	
	// ExecuteWaitList
	// Kernel is executed. Arguments are checked as being valid and execution 
	// parameters are computed. Execution is not complete when returning.
	//
	// Execution cannot proceed until all the antecedent events have completed.
	//
	// Event is set to NULL if execution fails, otherwise the event can be
	// used to track execution completion.
	result ExecuteWaitList(
		const	cl_command_queue	&cq, 
		const	int					&WaitListLength, 
				cl_event			*antecedents, 
				cl_event			*event);

private:

	// global_work_size
	// Compute the size of the n-dimensional execution domain as a multiple of 
	// the work items per work group.
	void global_work_size();

	cl_kernel	kernel_;				// Compiled OpenCL kernel, ready to execute
	bool		arguments_valid_;		// Tracks occurrence of an error in arguments
	int			argument_counter_;		// Count of arguments, when setting them sequentially
	cl_uint		work_dim_;				// Count of dimensions in the execution domain
	size_t		*local_work_size_;		// Corresponds with same named argument in clEnqueueNDRangeKernel
	size_t		*scalar_global_size_;	// Size of the global execution domain expressed as scalars
	size_t		*scalar_item_size_;		// Size of each work item in terms of scalars
	size_t		*global_work_size_;		// Size of the execution domain in terms of work items
};

#endif // _CLKERNEL_H_