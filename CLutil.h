/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.03
 *
 * Copyright 2013, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#ifndef _CL_UTIL_H_
#define _CL_UTIL_H_

#include <string>
using namespace std;

#include <CL/cl.h>
#include "result.h"

extern cl_int		g_last_cl_error;
extern cl_context	g_context;

// GetCLErrorString
// Returns error message in English
char* GetCLErrorString(const cl_int &err);

// GetPlatform
// Finds the first available OpenCL platform
result GetPlatform(cl_platform_id* platform);

// SetContext
// Creates a context solely for GPU devices, setting the 
// global variable g_context
result SetContext(const cl_platform_id &platform);

// GetDeviceCount
// Requires that g_context is valid
result GetDeviceCount(int* device_count); 

// GetDeviceList 
// Requires that g_context is valid
result GetDeviceList(cl_device_id** devices);

// AssembleSources
// Produces a single string containing the text of all the resources
// specified in the input array
result AssembleSources(
	const	int		*resources, 
	const	int		&resource_count, 
			string	*entire_program_source);

// CompileAll
// All kernels are compiled, for all available devices.
// Requires g_context and g_devices.
result CompileAll(
	const int			&device_count, 
	const cl_device_id	&devices);

// StartOpenCL
// Get OpenCL running, if possible
result StartOpenCL(int *device_count);

#endif  // _CL_UTIL_H_

