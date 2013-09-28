/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.03
 *
 * Copyright 2013, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#include <direct.h>

#include "util.h"
#include "clutil.h"
#include "device.h"
#include "CLKernel.h"

extern	int		g_device_count;

cl_int			g_last_cl_error = CL_SUCCESS;
cl_context		g_context		= NULL;

char* GetCLErrorString(const cl_int &err) {
	switch (err) {
		case CL_SUCCESS:                          return "Success!";
		case CL_DEVICE_NOT_FOUND:                 return "Device not found.";
		case CL_DEVICE_NOT_AVAILABLE:             return "Device not available";
		case CL_COMPILER_NOT_AVAILABLE:           return "Compiler not available";
		case CL_MEM_OBJECT_ALLOCATION_FAILURE:    return "Memory object allocation failure";
		case CL_OUT_OF_RESOURCES:                 return "Out of resources";
		case CL_OUT_OF_HOST_MEMORY:               return "Out of host memory";
		case CL_PROFILING_INFO_NOT_AVAILABLE:     return "Profiling information not available";
		case CL_MEM_COPY_OVERLAP:                 return "Memory copy overlap";
		case CL_IMAGE_FORMAT_MISMATCH:            return "Image format mismatch";
		case CL_IMAGE_FORMAT_NOT_SUPPORTED:       return "Image format not supported";
		case CL_BUILD_PROGRAM_FAILURE:            return "Program build failure";
		case CL_MAP_FAILURE:                      return "Map failure";
		case CL_INVALID_VALUE:                    return "Invalid value";
		case CL_INVALID_DEVICE_TYPE:              return "Invalid device type";
		case CL_INVALID_PLATFORM:                 return "Invalid platform";
		case CL_INVALID_DEVICE:                   return "Invalid device";
		case CL_INVALID_CONTEXT:                  return "Invalid context";
		case CL_INVALID_QUEUE_PROPERTIES:         return "Invalid queue properties";
		case CL_INVALID_COMMAND_QUEUE:            return "Invalid command queue";
		case CL_INVALID_HOST_PTR:                 return "Invalid host pointer";
		case CL_INVALID_MEM_OBJECT:               return "Invalid memory object";
		case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:  return "Invalid image format descriptor";
		case CL_INVALID_IMAGE_SIZE:               return "Invalid image size";
		case CL_INVALID_SAMPLER:                  return "Invalid sampler";
		case CL_INVALID_BINARY:                   return "Invalid binary";
		case CL_INVALID_BUILD_OPTIONS:            return "Invalid build options";
		case CL_INVALID_PROGRAM:                  return "Invalid program";
		case CL_INVALID_PROGRAM_EXECUTABLE:       return "Invalid program executable";
		case CL_INVALID_KERNEL_NAME:              return "Invalid kernel name";
		case CL_INVALID_KERNEL_DEFINITION:        return "Invalid kernel definition";
		case CL_INVALID_KERNEL:                   return "Invalid kernel";
		case CL_INVALID_ARG_INDEX:                return "Invalid argument index";
		case CL_INVALID_ARG_VALUE:                return "Invalid argument value";
		case CL_INVALID_ARG_SIZE:                 return "Invalid argument size";
		case CL_INVALID_KERNEL_ARGS:              return "Invalid kernel arguments";
		case CL_INVALID_WORK_DIMENSION:           return "Invalid work dimension";
		case CL_INVALID_WORK_GROUP_SIZE:          return "Invalid work group size";
		case CL_INVALID_WORK_ITEM_SIZE:           return "Invalid work item size";
		case CL_INVALID_GLOBAL_OFFSET:            return "Invalid global offset";
		case CL_INVALID_EVENT_WAIT_LIST:          return "Invalid event wait list";
		case CL_INVALID_EVENT:                    return "Invalid event";
		case CL_INVALID_OPERATION:                return "Invalid operation";
		case CL_INVALID_GL_OBJECT:                return "Invalid OpenGL object";
		case CL_INVALID_BUFFER_SIZE:              return "Invalid buffer size";
		case CL_INVALID_MIP_LEVEL:                return "Invalid mip-map level";
		default:                                  return "Unknown";
	}
}

result GetPlatform(cl_platform_id* platform) {
	*platform = NULL;

	cl_int			status;	
	cl_uint			platform_count;
	cl_platform_id* platforms;

	status = clGetPlatformIDs(0, NULL, &platform_count);
	if (status != CL_SUCCESS) {
		g_last_cl_error = status;
		return FILTER_NO_PLATFORM;
	}

	if (platform_count > 0)	{
		platforms = static_cast<cl_platform_id *> (malloc(platform_count * sizeof(cl_platform_id)));

		status = clGetPlatformIDs(platform_count, platforms, NULL);
		if (status != CL_SUCCESS) {
			g_last_cl_error = status;
			return FILTER_NO_PLATFORM;
		}

		*platform = platforms[0];
		return FILTER_OK;		
	} 

	return FILTER_NO_PLATFORM;
}

result SetContext(const cl_platform_id &platform) {
	cl_int					status = CL_SUCCESS;
	cl_context_properties	context_properties[3] = {CL_CONTEXT_PLATFORM, 
													 (cl_context_properties)platform, 
													 0};

	g_context = clCreateContextFromType(context_properties, 
									    CL_DEVICE_TYPE_GPU, 
									    NULL, 
									    NULL, 
									    &status);
	if (status != CL_SUCCESS) {  
		g_last_cl_error = status;
		return FILTER_NO_CONTEXT;
	}

	return FILTER_OK;		
}

result GetDeviceCount(int* device_count) {	
	if (g_context == NULL) {
		return FILTER_NO_CONTEXT;
	}

	cl_int	status = CL_SUCCESS;

	status = clGetContextInfo(g_context, 
							  CL_CONTEXT_NUM_DEVICES, 
							  sizeof(cl_uint),
							  device_count,
							  NULL);

	if (status != CL_SUCCESS) {  
		g_last_cl_error = status;
		return FILTER_CONTEXT_NUMBER_DEVICES_FAILED;
	}

	return FILTER_OK;		
}

result GetDeviceList(cl_device_id** devices) {
	// Getting the list of devices requires double-dipping CL_CONTEXT_DEVICES
	// first for size of return object, then the object
	
	if (g_context == NULL) {
		return FILTER_NO_CONTEXT;
	}

	cl_int cl_status = CL_SUCCESS;

	size_t device_list_size;
	cl_status = clGetContextInfo(g_context, CL_CONTEXT_DEVICES, 0, NULL, &device_list_size);
	if (cl_status != CL_SUCCESS) {  
		g_last_cl_error = cl_status;
		return FILTER_DEVICE_LIST_NOT_FOUND;
	}
	*devices = static_cast<cl_device_id*>(malloc(device_list_size));
	if (*devices == NULL) {
		return FILTER_MALLOC_FAILURE;
	}
	cl_status = clGetContextInfo(g_context, CL_CONTEXT_DEVICES, device_list_size, *devices, NULL);
	if (cl_status != CL_SUCCESS) {  
		g_last_cl_error = cl_status;
		return FILTER_NO_DEVICES_FOUND;
	}
	return FILTER_OK;		
}

result AssembleSources(const int* resources, const int& resource_count, string* entire_program_source) {
	// Iterates through the .cl source files that are stored as resources in the DLL
	// and concatenates them into a single string

	result status = FILTER_OK;
	string source;

	for (int i = 0; i < resource_count; ++i) {
		status = GetSourceFromResource(resources[i], &source);
		if (status != FILTER_OK) {
			return status;
		}
		entire_program_source->append(source);
	}

	return FILTER_OK ;	
}

result CompileAll(const int &device_count, const cl_device_id &devices) {
	// The OpenCL source code is spread amongst a number of 
	// .cl files encoded as resources. Each of these resources is 
	// fetched into a string from which the entire program is built 
	// and linked. Then each device in g_devices is given the program 
	// and a list of kernels to create.
	//
	// IMPORTANT: After changing any .cl file, manually compile Deathray.rc,
	// then link Deathray.

	result	status		= FILTER_OK;
	cl_int	cl_status	= CL_SUCCESS;

	const int resource_count = 4;
	const int resources[resource_count] = {RC_UTIL, // Always must be first
										   RC_NLM,
										   RC_NLM_SINGLE,
										   RC_NLM_MULTI,
										   };
	string entire_program_source;

	AssembleSources(&(resources[0]), resource_count, &entire_program_source);

	const char* entire_program_c_str = entire_program_source.c_str();
	cl_program program = clCreateProgramWithSource(g_context, 
												   1, 
												   &entire_program_c_str,
												   NULL,
												   &cl_status);
	if (cl_status != CL_SUCCESS) {  
		g_last_cl_error = cl_status;
		return FILTER_OPENCL_COMPILATION_FAILED;
	}

	cl_status = clBuildProgram(program, device_count, &devices, "-cl-fast-relaxed-math", NULL, NULL);
	if (cl_status != CL_SUCCESS) {
		g_last_cl_error = cl_status;
		size_t build_log_size;
		clGetProgramBuildInfo(program, devices, CL_PROGRAM_BUILD_LOG, 0, NULL, &build_log_size);
		char* build_log = static_cast<char*>(malloc(build_log_size * sizeof(char)));
		clGetProgramBuildInfo(program, devices, CL_PROGRAM_BUILD_LOG, build_log_size, build_log, NULL);
		free(build_log);
		return FILTER_OPENCL_KERNEL_DEVICE_BUILD_FAILED;
	}

	const int kernel_count = 4;
	const string kernels[kernel_count] = {"Initialise",
										  "NLMSingleFrameFourPixel",
										  "NLMMultiFrameFourPixel",
										  "NLMFinalise"
										  };
	for (int i = 0; i < device_count; ++i) {
		status = g_devices[i].KernelInit(program, kernel_count, &(kernels[0]));
		if (status != FILTER_OK) {
			return status;
		}
	}

	clUnloadCompiler();
	return status ;	
}

result StartOpenCL(int *device_count) {
	// Platform and devices have a lifetime of this function only. 

	result			status		= FILTER_OK;
	cl_int			cl_status	= CL_SUCCESS;
    cl_platform_id	platform	= NULL;

	if ((status = GetPlatform(&platform)) != FILTER_OK) {
		return status ;
	}

	if ((status = SetContext(platform)) != FILTER_OK) {
		return status ;
	}

	if ((status = GetDeviceCount(device_count)) != FILTER_OK) {
		return status ;
	}

	cl_device_id* devices = NULL;
	if ((status = GetDeviceList(&devices)) != FILTER_OK) {
		return status ;
	}

	if (*device_count == 0 || devices == NULL) {
		return FILTER_NO_GPUS_FOUND;
	}

	// Populate the global array of device objects with the devices
	// that were found and set up each device's command queue.
	g_device_count = *device_count;
	g_devices = new device[g_device_count];
	for (int i = 0; i < g_device_count; ++i) {
		g_devices[i].Init(static_cast<const cl_device_id&>(devices[i]));
	}

	status = CompileAll((g_device_count), static_cast<const cl_device_id&>(*devices)) ;
	return status ;		
}