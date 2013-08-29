/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.01
 *
 * Copyright 2013, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#ifndef MULTI_FRAME_H_
#define MULTI_FRAME_H_

#include <vector>

#include <CL/cl.h>
#include "result.h"
#include "CLKernel.h"
#include "buffer_map.h"
#include "MultiFrameRequest.h"

class MultiFrame {
public:
	MultiFrame();

	~MultiFrame() {}

	// Init
	// One-time configuration of this object to handle all multi-frame 
	// processing for the duration of the clip.
	result Init(
		const	int				&device_id,
		const	int				&temporal_radius,
		const	int				&width, 
		const	int				&height,
		const	int				&src_pitch,
		const	int				&dst_pitch,
		const	float			&h,
		const	int				&sample_expand,
		const	int				&linear);

	// SupplyFrameNumbers
	// Supplies a set of frame numbers, in object MultiFrameRequest
	// when Deathray object requests which frames should be copied
	// to the device.
	void SupplyFrameNumbers(
		const	int					&target_frame_number, 
				MultiFrameRequest	*required);

	// CopyTo
	// MultiFrameRequest contains the frame numbers and host
	// pointers of planes that have to be copied to device.
	result CopyTo(MultiFrameRequest *retrieved) ;
	
	// Execute
	// Runs all iterations of the temporal filter and the 
	// finalise kernel.
	result Execute();
	
	// CopyFrom
	// Copy filtered pixels to host, returning an event that
	// can be used to track completion of the copy.
	result CopyFrom(
		unsigned char	*dest,							  
		cl_event		*returned_);

private:

	// InitBuffers
	// Create and zero the intermediate averages plus weights buffers and create the destination buffer.
	result InitBuffers();

	// InitKernels
	// Configure global arguments for the filter and finalise kernels,
	// arguments that won't change over the duration of clip processing.
	result InitKernels(
		const int &sample_expand,
		const int &linear);

	// InitFrames
	// Create the Frame objects, one per step of the temporal filter.
	result InitFrames();

	// ZeroIntermediates
	// Before every new frame is processed the intermediate buffers must be zeroed.
	result ZeroIntermediates();

	// Frame
	// An object for each of the 2 * temporal_radius + 1 frames, all of which are processed separately.
	//
	// This allows a frame to stay in device memory without being repeatedly copied from host. 
	//
	// The usage of multiple Frame objects mimics a circular buffer. As frame number, n, progresses over
	// the lifetime of filtering a clip, each of the static set  of frame objects will take it in turns 
	// being the "target". e.g. for a cycle of 7 frames each object will do target processing once. 
	// Over this cycle of 7 frames, each Frame object only needs to perform a single copy of host 
	// data to the device.
	class Frame {
	public:
		Frame();
		~Frame() {}

		// Init
		// Tell the frame object to initialise its buffer
		result Init(
			const	int					&device_id,
			cl_command_queue			*cq, 
			const	CLKernel			&NLM_kernel,
			const	int					&width, 
			const	int					&height, 
			const	int					&pitch);

		// IsCopyRequired
		// Queries the Frame to discover if it needs data from the host
		// for the frame specified.
		bool IsCopyRequired(int &frame_number);

		// CopyTo
		// All frame objects are given the chance to copy host data to the device, if needed.
		//
		// This handles the once-per-cycle copying of host data to the device.
		result CopyTo(int &frame_number, const unsigned char* const source);

		// Plane
		// Allows the parent to query the frame known to be handling the target 
		// for the plane buffer it's using, so that all the other frames can use the same plane.
		//
		// Also returns the object's event for the copy of data to the buffer
		// so that other Frame objects can use the event as an antecedent.
		void Plane(int *plane, cl_event *target_copied);

		// Execute
		// Performs the NLM pass.
		//
		// During each cycle the client instructs a single frame object that it is 
		// handling the target frame. The id of the frame object handling the target frame
		// progresses circularly around the "ring" of Frame objects, as the clip is processed.
		result Execute(
			const	bool		&is_sample_equal_to_target, 
					cl_event	*antecedent, 
					cl_event	*executed);

	private:

		int device_id_			;	// device executing the kernels
		cl_command_queue cq_	;	// command queue shared by all Frame objects and client object
		CLKernel NLM_kernel_	;	// each frame sets arguments for a kernel shared by all
		int frame_number_		;	// frame being processed
		int plane_				;	// buffer for the frame being processed
		int width_				;	// width of plane's content
		int height_				;	// height of plane's content
		int pitch_				;	// host plane format allows each row to be potentially longer than width_
		cl_event copied_		;	// tracks completion of the copy from host to device of the Frame's sample plane
		cl_event wait_list_[2]	;	// used during execution to track completion of copying of target and sample planes
		int frame_used_			;	// tracks count of times plane data has been copied to device - enables kludge
	};


	int device_id_				;	// device used to execute the filter kernels
	int	temporal_radius_		;	// count of frames either side of target frame that will be included in multi-frame filtering
	vector<Frame> frames_		;	// set of frame planes including target
	int target_frame_number_	;	// frame to be filtered
	int dest_plane_				;	// dedicated buffer for destination plane
	int averages_				;	// intermediate averages buffer
	int	weights_				;	// intermediate weights buffer
	int width_					;	// width of plane's content
	int height_					;	// height of plane's content
	int src_pitch_				;	// host plane format allows each row to be potentially longer than width_
	int dst_pitch_				;	// host plane format allows each row to be potentially longer than width_
	float h_					;	// strength of noise reduction
	cl_command_queue cq_		;	// device object for queue management and synchronisation
	size_t intermediate_width_	;	// width of intermediate buffers, rounded-up to 32 pixels, expressed as 4-pixel strips
	size_t intermediate_height_	;	// height of intermediate buffers, rounded-up to 32 rows
	CLKernel NLM_kernel_		;	// kernel that performs NLM computations, once per sample plane
	CLKernel finalise_kernel_	;	// single invocation of this kernel to average all samples
	cl_event copied_			;	// used to track the final copy to the device - at least one frame is copied to the device
	cl_event executed_			;	// finalise kernel is executed synchronously, but event is used for asynchronous copy back to host

	// Kernel needs to know whether the plane it is sampling from is the target plane
	static const int k_sample_equals_target = 1;
	static const int k_sample_is_not_target = 0;
};

#endif // MULTI_FRAME_H_