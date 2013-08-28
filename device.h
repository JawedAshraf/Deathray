/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.00
 *
 * Copyright 2011, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <map>
#include <CL/cl.h>
#include "buffer_map.h"


// device
// An object for each installed device that can be found and used, which
// records the capabilities and status of each device.
//
// The DLL uses a global array of pointers to this class, g_devices. Each object
// is constructed in an undefined state. 
//
// Init must be used first.
class device {
public:

	// Constructor
	// Always constructed in an undefined state.
	// Client must create an array of device objects
	// before configuring each one individually.
	device::device();	

	// Destructor
	// Deletes extant buffers
	~device();

	// Init
	// Record the new device and create a command queue for it
	void Init(const cl_device_id &single_device);

	// KernelInit
	// Compile the set of kernels for the device
	result KernelInit(
		const cl_program	&program,
		const size_t		&kernel_count, 
		const string		*kernels);

	// kernel accessor
	// For clients that want to set up a kernel along with its arguments
	// and enqueue it immediately, the globally shared instance of the
	// kernel can be used. It is not thread safe.
	cl_kernel kernel(const string &kernel);

	// NewKernelInstance
	// Returns an independent instance of a kernel. This allows
	// multiple objects to each access the same compiled kernel
	// whilst having independent arguments. This avoids the 
	// race condition that otherwise exists when multiple threads
	// or objects set arguments on a named kernel, concurrently.
	cl_kernel NewKernelInstance(const string &kernel);

	// cq
	// Returns a new command queue. 
	// This command queue can be shared by multiple objects or 
	// used exclusively by the object that calls this method.
	cl_command_queue		cq();

	buffer_map				buffers_;	// set of buffers on the device - TODO make private and create methods in this class

private:
	cl_device_id			id_;		// sequence number of the device
	map<string, cl_kernel>	kernel_;	// set of kernel objects that have been pre-compiled
	cl_program				program_;	// program object used to generate new instances of named kernels
};

extern device*				g_devices ;

#endif // _DEVICE_H_
