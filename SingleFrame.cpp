/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.00
 *
 * Copyright 2011, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#include "SingleFrame.h"
#include "device.h"
#include "buffer_map.h"

extern	int		g_device_count;
extern	device	*g_devices;

extern	int		g_gaussian;

SingleFrame::SingleFrame() {
	device_id_		= 0;
	width_			= 0;
	height_			= 0;
	src_pitch_		= 0;
	dst_pitch_		= 0;
	source_plane_	= 0;
	dest_plane_		= 0;
}

result SingleFrame::Init(
	const	int		&device_id,
	const	int		&width, 
	const	int		&height,
	const	int		&src_pitch,
	const	int		&dst_pitch,
	const	float	&h,
	const	int		&sample_expand) {

	if (device_id >= g_device_count) return FILTER_ERROR;

	result status = FILTER_OK;

	device_id_		= device_id;
	width_			= width;
	height_			= height;
	src_pitch_		= src_pitch;
	dst_pitch_		= dst_pitch;
	cq_				= g_devices[device_id_].cq();

	if (width_ == 0 || height_ == 0 || src_pitch_ == 0 || dst_pitch_ == 0 || h == 0 ) return FILTER_INVALID_PARAMETER;

	status = g_devices[0].buffers_.AllocPlane(cq_, width_, height_, &source_plane_);
	status = max(status, g_devices[0].buffers_.AllocPlane(cq_, width_, height_, &dest_plane_));
	if (status != FILTER_OK) return status;

	kernel_ = CLKernel(device_id_, "NLMSingleFrameFourPixel");

	kernel_.SetArg(sizeof(cl_mem), g_devices[device_id_].buffers_.ptr(source_plane_));
	kernel_.SetArg(sizeof(int), &width_);
	kernel_.SetArg(sizeof(int), &height_);
	kernel_.SetArg(sizeof(float), &h);
	kernel_.SetArg(sizeof(int), &sample_expand);
	kernel_.SetArg(sizeof(cl_mem), g_devices[device_id_].buffers_.ptr(g_gaussian));
	kernel_.SetArg(sizeof(cl_mem), g_devices[device_id_].buffers_.ptr(dest_plane_));

	if (kernel_.arguments_valid()) {
		kernel_.set_work_dim(2);
		const size_t set_local_work_size[2]		= {8, 32};
		const size_t set_scalar_global_size[2]	= {width_, height_};
		const size_t set_scalar_item_size[2]	= {4, 1};

		kernel_.set_local_work_size(set_local_work_size);
		kernel_.set_scalar_global_size(set_scalar_global_size);
		kernel_.set_scalar_item_size(set_scalar_item_size);

		return FILTER_OK;
	}

	return FILTER_KERNEL_ARGUMENT_ERROR;
}

result SingleFrame::CopyTo(const unsigned char *source) {
	return g_devices[device_id_].buffers_.CopyToPlaneAsynch(source_plane_,
															*source, 
															width_, 
															height_, 
															src_pitch_, 
															&copied_to_);

}

result SingleFrame::Execute() {
	return kernel_.ExecuteAsynch(cq_, &copied_to_, &executed_);
}

result SingleFrame::CopyFrom(
	unsigned char	*dest,							  
	cl_event		*returned) {

	return g_devices[device_id_].buffers_.CopyFromPlaneAsynch(dest_plane_,
															  width_,
															  height_, 
															  dst_pitch_, 
															  &executed_, 
															  returned,
															  dest);
}
