/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.01
 *
 * Copyright 2013, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#ifndef _BUFFER_MAP_H_
#define _BUFFER_MAP_H_

#include <map>
#include "buffer.h"
#include "util.h"

// buffer_map
// Set of buffers currently in use on a device.
//
// Buffers can be either plain old data or 
// 8-bit pixels of luma or chroma data, 
// known as "plane".
class buffer_map {
public:
	buffer_map() {}
	~buffer_map() {}

	// AllocBuffer
	// Creates a new OpenCL buffer on the device and puts it in the map
	// of open buffers.
	// Format of the buffer is 1D scalars of any type.
	// No zero-padding is required.
	result AllocBuffer(
		const	cl_command_queue	&cq,			// device specific command queue
		const	size_t				&bytes,			// size in bytes
				int					*new_index);	// map index of the new buffer

	// CopyToBuffer
	// device buffer is populated with data from a host buffer
	result CopyToBuffer(
		const	int		&index,			// destination buffer
		const	void	*host_buffer, 	// host's buffer as source		
		const	size_t	&bytes);		// size in bytes to copy

	// CopyFromBuffer
	// Data from device buffer is copied to a host buffer
	result CopyFromBuffer(
		const	int		&index,			// source buffer
		const	size_t	&bytes,	 		// size in bytes to copy
				void	*host_buffer);	// host's buffer as destination	

	// AllocPlane
	// Creates a new OpenCL buffer on the device and puts it in the map
	// of open buffers.
	// Format of the buffer is 2D pixels, with pixels organised as strips
	// of 4 horizontally.
	// width and height represent native size of data, exclusive of padding.
	result AllocPlane(
		const	cl_command_queue	&cq,			// device specific command queue
		const	int					&width,			// width in pixels
		const	int					&height,		// rows
				int					*new_index);	// map index of the new buffer

	// CopyToPlane
	// Copy pixels from host buffer to plane
	// Method returns immediately, i.e. copy completion 
	// is not guaranteed upon return
	result CopyToPlane(
		const	int		&index,			// index of the device buffer
		const	byte	&host_buffer, 	// host's buffer of pixels in row major layout			
		const	int		&host_cols,	 	// count of pixels per row to be copied		
		const	int		&host_rows,	 	// count of rows to be copied		
		const	int		&host_pitch);	// size in pixels of each row of host buffer

	// CopyToPlaneAsynch
	// Copy pixels from host buffer to plane.
	// Method returns immediately, i.e. copy completion 
	// is not guaranteed upon return. Event can be used
	// to discern when copy has finished.
	result CopyToPlaneAsynch(
		const	int			&index,			// index of the device buffer
		const	byte		&host_buffer, 	// host's buffer of pixels in row major layout			
		const	int			&host_cols,	 	// count of pixels per row to be copied				
		const	int			&host_rows,	 	// count of rows to be copied				
		const	int			&host_pitch,	// size in pixels of each row of host buffer
				cl_event	*event);		// event to track completion of this copy

	// CopyFromPlane
	// Copy pixels from plane to host buffer.
	// Method returns immediately, i.e. copy completion 
	// is not guaranteed upon return
	result CopyFromPlane(
		const	int		&index,			// index of the device buffer
		const	int		&host_cols,	 	// count of pixels per row to be copied		
		const	int		&host_rows,	 	// count of rows to be copied							
		const	int		&host_pitch,	// size in pixels of each row of host buffer
				byte	*host_buffer);	// host's buffer of pixels in row major layout			

	// CopyFromPlaneAsynch
	// Copy pixels from plane to host buffer.
	// Method returns immediately, i.e. copy completion 
	// is not guaranteed upon return. 
	// 
	// This copy will not start until the antecedent event's 
	// completion.
	//
	// Event can be used to discern when copy has finished.
	result CopyFromPlaneAsynch(
		const	int			&index,			// index of the device buffer
		const	int			&host_cols,	 	// count of pixels per row to be copied				
		const	int			&host_rows,	 	// count of rows to be copied							
		const	int			&host_pitch,	// size in pixels of each row of host buffer
		const	cl_event	*antecedent,	// event that must complete before this copy can start
				cl_event	*event,			// event to track completion of this copy
				byte		*host_buffer);	// host's buffer of pixels in row major layout

	// Destroy
	// Destroys buffer entry in map and releases the device-allocated buffer
	void Destroy(const int &index);

	// DestroyAll
	// Destroys all device buffers in the map
	void DestroyAll();

	// ptr
	// Pointer for the memory buffer. Used when setting arguments for
	// a kernel call.
	cl_mem* ptr(const int &index);

private:

	// NewIndex
	// Returns index that can be used for a new buffer
	int	NewIndex();

	// Append
	// Adds an existing buffer into the map of buffers, and provides
	// the caller with the new buffer's index
	result Append(mem **new_mem, int *new_index);

	// ValidIndex
	// Checks that the buffer has been setup
	bool ValidIndex(const int &index);

	map<int, mem*> buffer_map_;
};

#endif // _BUFFER_MAP_H_