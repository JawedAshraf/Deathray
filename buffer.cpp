/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.00
 *
 * Copyright 2011, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#include "buffer.h"
#include "CLutil.h"
#include "util.h"

extern cl_context g_context ;

// mem
mem::mem() {
	mem_ = NULL;
	valid_  = false;
}

mem::~mem() {

	if (mem_ == NULL) return;

	cl_uint reference_count = 0;
	clGetMemObjectInfo(mem_,
					   CL_MEM_REFERENCE_COUNT,
					   sizeof(cl_uint),
					   &reference_count,
					   0);

	for (cl_uint i = 0; i < reference_count; ++i) {
		clReleaseMemObject(mem_);
	}

	mem_ = NULL;
	valid_ = false;
}

cl_mem mem::obj() {
	return mem_;
}

cl_mem* mem::ptr() {
	return &mem_;
}

// buffer
void buffer::Init(
	const cl_command_queue	&cq,
	const size_t			&bytes) {

	cl_int cl_status = CL_SUCCESS;

	cq_ = cq;
	bytes_ = bytes;

	mem_ = clCreateBuffer(g_context,
						  CL_MEM_READ_WRITE,
						  bytes,
						  NULL,
						  &cl_status);
	if (cl_status != CL_SUCCESS) {  
		g_last_cl_error = cl_status;
		return;
	}

	valid_ = true;
}

result buffer::CopyTo(
	const void		*host_buffer,
	const size_t	&bytes) {

		if (!valid_) return FILTER_ERROR;

	valid_ = false;

	if (bytes > bytes_) return FILTER_INVALID_PARAMETER;

	cl_int cl_status = CL_SUCCESS;

	cl_status = clEnqueueWriteBuffer(cq_,
									 mem_,
									 CL_TRUE,
									 0,
									 bytes,
									 host_buffer,
									 0,
									 NULL,
									 NULL);
	if (cl_status != CL_SUCCESS) {  
		g_last_cl_error = cl_status;
		return FILTER_ERROR;
	}

	valid_ = true;
	return FILTER_OK;
}

result buffer::CopyFrom(
	const	size_t	&bytes,	 	 			
			void	*host_buffer) {

	if (bytes > bytes_) return FILTER_INVALID_PARAMETER;
	if (!valid_)		return FILTER_ERROR;

	cl_int cl_status = CL_SUCCESS;

	cl_status = clEnqueueReadBuffer(cq_,
									mem_,
									CL_TRUE,
									0,
									bytes,
									host_buffer,
									0,
									NULL,
									NULL);
	if (cl_status != CL_SUCCESS) {  
		g_last_cl_error = cl_status;
		return FILTER_ERROR;
	}
	return FILTER_OK;
}

// plane
void plane::Init(
	const cl_command_queue	&cq,
	const int				&width, 
	const int				&height,
	const int				&width_constraint,
	const int				&height_constraint) {

	cl_int cl_status = CL_SUCCESS;

	cq_ = cq;
	GetFrameDimensions(width,
					   height, 
					   width_constraint, 
					   height_constraint, 
					   &width_, 
					   &height_);

	mem_ = clCreateImage2D(g_context,
						   CL_MEM_READ_WRITE,
						   &GetFormatPixel(),
						   width_,
						   height_,
						   0,
						   NULL,
						   &cl_status);
	if (cl_status != CL_SUCCESS) {  
		g_last_cl_error = cl_status;
		return;
	}

	valid_ = true;
}

result plane::CopyTo(
	const unsigned char		&host_buffer, 			
	const int				&host_cols,	 			
	const int				&host_rows,	 			
	const int				&host_pitch) {
	
	if (!valid_) return FILTER_INVALID_PLANE_BUFFER_STATE;

	valid_ = false;

	cl_int cl_status = CL_SUCCESS;

	size_t zero_offset[] = {0, 0, 0}; 
	size_t copy_region[] = {ByPowerOf2(host_cols, 2) >> 2, host_rows, 1};
	cl_status = clEnqueueWriteImage(cq_,
									mem_,
									CL_FALSE,
									zero_offset,
									copy_region,
									host_pitch,
									0,
									&host_buffer,
									0,
									NULL,
									NULL);
	if (cl_status != CL_SUCCESS) {  
		g_last_cl_error = cl_status;
		return FILTER_COPYING_TO_PLANE_FAILED;
	}

	valid_ = true;
	return FILTER_OK;
}

result plane::CopyToAsynch(
	const	unsigned char	&host_buffer, 			
	const	int				&host_cols,	 			
	const	int				&host_rows,	 			
	const	int				&host_pitch,
			cl_event		*event) {
	
	if (!valid_) return FILTER_INVALID_PLANE_BUFFER_STATE;

	valid_ = false;

	cl_int cl_status = CL_SUCCESS;

	size_t zero_offset[] = {0, 0, 0}; 
	size_t copy_region[] = {ByPowerOf2(host_cols, 2) >> 2, host_rows, 1};
	cl_status = clEnqueueWriteImage(cq_,
									mem_,
									CL_FALSE,
									zero_offset,
									copy_region,
									host_pitch,
									0,
									&host_buffer,
									0,
									NULL,
									event);
	if (cl_status != CL_SUCCESS) {  
		g_last_cl_error = cl_status;
		return FILTER_COPYING_TO_PLANE_FAILED;
	}

	valid_ = true;
	return FILTER_OK;
}

result plane::CopyFrom(
	const int				&host_cols,	 			
	const int				&host_rows,	 			
	const int				&host_pitch,
		  unsigned char		*host_buffer) {

	if (!valid_) return FILTER_INVALID_PLANE_BUFFER_STATE;

	cl_int cl_status = CL_SUCCESS;

	size_t zero_offset[] = {0, 0, 0}; 
	size_t copy_region[] = {ByPowerOf2(host_cols,2) >> 2, host_rows, 1};

	cl_status = clEnqueueReadImage(cq_,
								   mem_,
								   CL_FALSE,
								   zero_offset,
								   copy_region,
								   host_pitch,
								   0,
								   host_buffer,
								   0,
								   NULL,
								   NULL);
	if (cl_status != CL_SUCCESS) {  
		g_last_cl_error = cl_status;
		return FILTER_COPYING_FROM_PLANE_FAILED;
	}

	return FILTER_OK;
}

result plane::CopyFromAsynch(
	const	int				&host_cols,	 			
	const	int				&host_rows,	 			
	const	int				&host_pitch,
	const	cl_event		*antecedent,
			cl_event		*event,
			unsigned char	*host_buffer) {

	if (!valid_) return FILTER_INVALID_PLANE_BUFFER_STATE;

	cl_int cl_status = CL_SUCCESS;

	size_t zero_offset[] = {0, 0, 0}; 
	size_t copy_region[] = {ByPowerOf2(host_cols,2) >> 2, host_rows, 1};

	cl_status = clEnqueueReadImage(cq_,
								   mem_,
								   CL_FALSE,
								   zero_offset,
								   copy_region,
								   host_pitch,
								   0,
								   host_buffer,
								   1,
								   antecedent,
								   event);
	if (cl_status != CL_SUCCESS) {  
		g_last_cl_error = cl_status;
		return FILTER_COPYING_FROM_PLANE_FAILED;
	}

	return FILTER_OK;
}

cl_image_format GetFormatPixel() {
	// Image processing uses floating point arithmetic.
	// The device automatically converts integers between the host
	// format of 0 to 255 and the range 0.f to 1.f.

	cl_image_format format;

	// This merely describes four pixels from the plane, not the colour space.
	// It means four pixels are addressed as a single element by the device.
	format.image_channel_order		= CL_RGBA;
	// unsigned normalised byte, i.e. 0 to 255 seen by host, is 0.f to 1.f in kernel
	format.image_channel_data_type	= CL_UNORM_INT8;	

	return format;
}
