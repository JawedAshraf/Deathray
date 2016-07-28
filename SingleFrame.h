/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.04
 *
 * Copyright 2013, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#ifndef _SINGLE_FRAME_
#define _SINGLE_FRAME_

#include <windows.h>
#include <CL/cl.h>
#include "avisynth.h"
#include "CLKernel.h"
#include "result.h"

class SingleFrame
{
public:
	SingleFrame();

	~SingleFrame() {}

	// Init
	// Setup the static source and destination buffers on the device
	// and configure the kernel and its arguments, which do not
	// change over the duration of clip processing.
	result Init(
		const	int		&device_id,
		const	int		&width, 
		const	int		&height,
		const	int		&src_pitch,
		const	int		&dst_pitch,
		const	float	&h,
		const	int		&sample_expand,
		const	int		&linear,
		const	int		&correction,
		const	int		&target_min,
		const	int		&balanced);

	// CopyTo
	// Copy the plane from host to device.
	result CopyTo(const unsigned char *source);

	// Execute
	// Perform NLM computation.
	result Execute();

	// CopyFrom
	// Copy the plane of filtered pixels from the device
	// to the destination buffer on the host.
	result CopyFrom(
		unsigned char	*dest,
		cl_event		*returned);

private:
	int device_id_		;	// device used to execute the filter kernels
	int width_			;	// width of plane's content
	int height_			;	// height of plane's content
	int src_pitch_		;	// host plane format allows each row to be potentially longer than width_
	int dst_pitch_		;	// host plane format allows each row to be potentially longer than width_
	int source_plane_	;	// dedicated buffer for source plane
	int dest_plane_		;	// dedicated buffer for destination plane
	cl_command_queue cq_;	// device is used asynchronously so it is more a pool of commands rather than a queue
	CLKernel kernel_	;	// non local means kernel executed on device
	cl_event copied_to_ ;	// source buffer is copied to device asynchronously
	cl_event executed_	;	// kernel is executed asynchronously
};

#endif // _SINGLE_FRAME_