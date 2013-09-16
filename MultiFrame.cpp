/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.02
 *
 * Copyright 2013, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#include <math.h>

#include "device.h"
#include "buffer.h"
#include "buffer_map.h"
#include "CLKernel.h"
#include "MultiFrame.h"

extern	int			g_device_count;
extern	device		*g_devices;
extern	cl_context	g_context;
extern	int			g_gaussian;

MultiFrame::MultiFrame() {
	device_id_			= 0;
	temporal_radius_	= 0;
	frames_.clear();
	dest_plane_			= 0;
	width_				= 0;
	height_				= 0;
	src_pitch_			= 0;
	dst_pitch_			= 0;
}

result MultiFrame::Init(
	const	int				&device_id,
	const	int				&temporal_radius,
	const	int				&width, 
	const	int				&height,
	const	int				&src_pitch,
	const	int				&dst_pitch,
	const	float			&h,
	const	int				&sample_expand) {

	if (device_id >= g_device_count) return FILTER_ERROR;

	result status = FILTER_OK;

	device_id_			= device_id;
	temporal_radius_	= temporal_radius;
	width_				= width;	
	height_				= height;	
	src_pitch_			= src_pitch;
	dst_pitch_			= dst_pitch;
	h_					= h;
	cq_					= g_devices[device_id_].cq();

	if (width_ == 0 || height_ == 0 || src_pitch_ == 0 || dst_pitch_ == 0 || h == 0 ) return FILTER_INVALID_PARAMETER;

	status = InitBuffers();
	if (status != FILTER_OK) return status;
	status = InitKernels(sample_expand);
	if (status != FILTER_OK) return status;
	status = InitFrames();

	return status;						
}

result MultiFrame::InitBuffers() {
	result status = FILTER_OK;

	// Buffer is sized upwards to the next highest multiple of 32
	// horizontally and vertically because tile size is 32x32
	intermediate_width_ = ByPowerOf2(width_, 5) >> 2;
	intermediate_height_ = ByPowerOf2(height_, 5);
	const size_t bytes = intermediate_width_ * intermediate_height_ * sizeof(float) << 2;
	status = g_devices[device_id_].buffers_.AllocBuffer(cq_, bytes, &averages_);
	if (status != FILTER_OK) return status;
	status = g_devices[device_id_].buffers_.AllocBuffer(cq_, bytes, &weights_);
	if (status != FILTER_OK) return status;

	status = g_devices[device_id_].buffers_.AllocPlane(cq_, width_, height_, &dest_plane_);

	return status;
}

result MultiFrame::InitKernels(const int &sample_expand) {
	NLM_kernel_ = CLKernel(device_id_, "NLMMultiFrameFourPixel");
	NLM_kernel_.SetNumberedArg(3, sizeof(int), &width_);
	NLM_kernel_.SetNumberedArg(4, sizeof(int), &height_);
	NLM_kernel_.SetNumberedArg(5, sizeof(float), &h_);
	NLM_kernel_.SetNumberedArg(6, sizeof(int), &sample_expand);
	NLM_kernel_.SetNumberedArg(7, sizeof(cl_mem), g_devices[device_id_].buffers_.ptr(g_gaussian));
	NLM_kernel_.SetNumberedArg(8, sizeof(int), &intermediate_width_);
	NLM_kernel_.SetNumberedArg(9, sizeof(cl_mem), g_devices[device_id_].buffers_.ptr(averages_));
	NLM_kernel_.SetNumberedArg(10, sizeof(cl_mem), g_devices[device_id_].buffers_.ptr(weights_));

	const size_t set_local_work_size[2]		= {8, 32};
	const size_t set_scalar_global_size[2]	= {width_, height_};
	const size_t set_scalar_item_size[2]	= {4, 1};

	if (NLM_kernel_.arguments_valid()) {
		NLM_kernel_.set_work_dim(2);
		NLM_kernel_.set_local_work_size(set_local_work_size);
		NLM_kernel_.set_scalar_global_size(set_scalar_global_size);
		NLM_kernel_.set_scalar_item_size(set_scalar_item_size);
	} else {
		return FILTER_KERNEL_ARGUMENT_ERROR;
	}

	finalise_kernel_ = CLKernel(device_id_, "NLMFinalise");
	finalise_kernel_.SetNumberedArg(1, sizeof(cl_mem), g_devices[device_id_].buffers_.ptr(averages_));
	finalise_kernel_.SetNumberedArg(2, sizeof(cl_mem), g_devices[device_id_].buffers_.ptr(weights_));
	finalise_kernel_.SetNumberedArg(3, sizeof(int), &intermediate_width_);
	finalise_kernel_.SetNumberedArg(4, sizeof(cl_mem), g_devices[device_id_].buffers_.ptr(dest_plane_));

	if (finalise_kernel_.arguments_valid()) {
		finalise_kernel_.set_work_dim(2);
		finalise_kernel_.set_local_work_size(set_local_work_size);
		finalise_kernel_.set_scalar_global_size(set_scalar_global_size);
		finalise_kernel_.set_scalar_item_size(set_scalar_item_size);
	} else {
		return FILTER_KERNEL_ARGUMENT_ERROR;
	}

	return FILTER_OK;						
}

result MultiFrame::InitFrames() {	
	const int frame_count = 2 * temporal_radius_ + 1;
	frames_.reserve(frame_count);
	for (int i = 0; i < frame_count; ++i) {
		Frame new_frame;
		frames_.push_back(new_frame);
		frames_[i].Init(device_id_, &cq_, NLM_kernel_, width_, height_, src_pitch_);
	}

	if (frames_.size() != frame_count)
		return FILTER_MULTI_FRAME_INITIALISATION_FAILED;

	return FILTER_OK;
}

result MultiFrame::ZeroIntermediates() {
	result status = FILTER_OK;

	CLKernel Intermediates = CLKernel(device_id_, "Zero");

	Intermediates.SetArg(sizeof(cl_mem), g_devices[device_id_].buffers_.ptr(averages_));

	if (Intermediates.arguments_valid()) {
		const size_t set_local_work_size[1]		= {256};
		const size_t set_scalar_global_size[1]	= {intermediate_width_ * intermediate_height_ << 2};
		const size_t set_scalar_item_size[1]	= {4};

		Intermediates.set_work_dim(1);
		Intermediates.set_local_work_size(set_local_work_size);
		Intermediates.set_scalar_global_size(set_scalar_global_size);
		Intermediates.set_scalar_item_size(set_scalar_item_size);
		status = Intermediates.Execute(cq_, NULL);
		if (status != FILTER_OK) return status;
	} else {
		return FILTER_KERNEL_ARGUMENT_ERROR;
	}

	Intermediates.SetNumberedArg(0, sizeof(cl_mem), g_devices[device_id_].buffers_.ptr(weights_));
	if (Intermediates.arguments_valid()) {
		status = Intermediates.Execute(cq_, NULL);
		if (status != FILTER_OK) return status;
	} else {
		return FILTER_KERNEL_ARGUMENT_ERROR;
	}
	clFinish(cq_);
	return status;						
}

void MultiFrame::SupplyFrameNumbers(
	const	int					&target_frame_number, 
			MultiFrameRequest	*required) {

	target_frame_number_ = target_frame_number;

	for (int i = -temporal_radius_, frame_id = 0; i <= temporal_radius_; ++i, ++frame_id) {
		int frame_number = target_frame_number_ - ((target_frame_number_ - i + temporal_radius_) % frames_.size()) + temporal_radius_;
		if (frames_[frame_id].IsCopyRequired(frame_number))
			required->Request(frame_number);	
	}
}

result MultiFrame::CopyTo(MultiFrameRequest *retrieved) {
	result status = FILTER_OK;

	status = ZeroIntermediates();
	if (status != FILTER_OK) return status;

	for (int i = -temporal_radius_, frame_id = 0; i <= temporal_radius_; ++i, ++frame_id) {
		int frame_number = target_frame_number_ - ((target_frame_number_ - i + temporal_radius_) % frames_.size()) + temporal_radius_;
		status = frames_[frame_id].CopyTo(frame_number, retrieved->Retrieve(frame_number));
		if (status != FILTER_OK) return status;
	}
	clFinish(cq_);
	return status;
}

result MultiFrame::Execute() {
	result status = FILTER_OK;

	// Query the Frame object handling the target frame to get the plane for the other Frames to use
	int target_frame_id = (target_frame_number_ + temporal_radius_) % frames_.size();

	int target_frame_plane; 		
	cl_event copying_target, executed;
	frames_[target_frame_id].Plane(&target_frame_plane, &copying_target);

	NLM_kernel_.SetNumberedArg(0, sizeof(cl_mem), g_devices[device_id_].buffers_.ptr(target_frame_plane));

	cl_event *filter_events = new cl_event[frames_.size()];
	for (int i = 0; i < 2 * temporal_radius_ + 1; ++i) {
		bool sample_equals_target = i == target_frame_id;
		status = frames_[i].Execute(sample_equals_target, &copying_target, &executed);
		filter_events[i] = executed;
		if (status != FILTER_OK) return status;
	}

	finalise_kernel_.SetNumberedArg(0, sizeof(cl_mem), g_devices[device_id_].buffers_.ptr(target_frame_plane));
	status = finalise_kernel_.ExecuteWaitList(cq_, frames_.size(), filter_events, &executed_);
	if (status != FILTER_OK) return status;

	clFinish(cq_);
	return status;
}

result MultiFrame::CopyFrom(
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

// Frame
MultiFrame::Frame::Frame() {
	frame_number_			= 0;
	plane_					= 0;	
	width_					= 0;
	height_					= 0;
	pitch_					= 0;
}

result MultiFrame::Frame::Init(
	const	int					&device_id,
			cl_command_queue	*cq, 
	const	CLKernel			&NLM_kernel,
	const	int					&width, 
	const	int					&height, 
	const	int					&pitch) {

	// Setting this frame's NLM_kernel_ to the client's kernel object means all frames share the 
	// same instance, and therefore each Frame object only needs to do minimal argument
	// setup.
						
	device_id_	= device_id;
	cq_			= *cq;
	NLM_kernel_	= NLM_kernel;
	width_		= width;
	height_		= height;
	pitch_		= pitch;
	frame_used_	= 0;

	return g_devices[device_id_].buffers_.AllocPlane(cq_, width_, height_, &plane_);
}

bool MultiFrame::Frame::IsCopyRequired(int &frame_number) {
	// BUG: erroneous frames will appear early in clip
	// FIX: frame_used_ < 3 is a kludge.
	if (frame_used_ < 3 || (frame_number != frame_number_)) 
		return true;
	else
		return false;
}

result MultiFrame::Frame::CopyTo(int &frame_number, const unsigned char* const source) {
	result status = FILTER_OK;

	if (IsCopyRequired(frame_number)) {
		frame_number_ = frame_number;
		status = g_devices[device_id_].buffers_.CopyToPlaneAsynch(plane_, *source, width_, height_, pitch_, &copied_);
	} else {
		copied_ = NULL;
	}
	++frame_used_;
	return status;
}

void MultiFrame::Frame::Plane(int *plane, cl_event *target_copied) {
	*plane = plane_;
	*target_copied = copied_;
}

result MultiFrame::Frame::Execute(
	const	bool		&is_sample_equal_to_target, 
			cl_event	*antecedent, 
			cl_event	*executed) {
	result status = FILTER_OK;

	int sample_equals_target = is_sample_equal_to_target ? k_sample_equals_target : k_sample_is_not_target;

	NLM_kernel_.SetNumberedArg(1, sizeof(cl_mem), g_devices[device_id_].buffers_.ptr(plane_));
	NLM_kernel_.SetNumberedArg(2, sizeof(int), &sample_equals_target);

	if (copied_ == NULL && *antecedent == NULL) { // target and sample frame are both on device from prior iteration
		status = NLM_kernel_.Execute(cq_, executed);
	} else if (*antecedent == NULL) { // target is on device, awaiting sample
		status = NLM_kernel_.ExecuteAsynch(cq_, &copied_, executed);
	} else if (copied_ == NULL) { // sample is on device, awaiting target
		status = NLM_kernel_.ExecuteAsynch(cq_, antecedent, executed);
	} else { // awaiting sample and target planes
		wait_list_[0] = copied_;
		wait_list_[1] = *antecedent;
		status = NLM_kernel_.ExecuteWaitList(cq_, 2, wait_list_, executed);
	}

	return status;
}
