/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.03
 *
 * Copyright 2013, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <CL/cl.h>
#include "result.h"


// mem
// Abstract class used to track the lifetime of all types of buffer on the device.
class mem {
public:
	mem();
	~mem();

	// obj
	// Returns the OpenCL memory buffer. Used to manipulate the 
	// OpenCL mem object.
	cl_mem obj();

	// ptr
	// Pointer for the memory buffer. Used when setting arguments for
	// a kernel call.
	cl_mem* ptr();

	// Init
	// This class is abstract
	virtual void Init() = NULL;

	// valid
	// Indicates that the buffer is ready to be used
	bool valid() {return valid_;}

protected:
	bool			 valid_ ;	// buffer is not usable unless set up correctly
	cl_command_queue cq_    ;	// buffer is associated with a single device
	cl_mem			 mem_   ;	// OpenCL buffer object
};

// buffer
// Scalar or vector float, int, uint etc. buffer on the device
// Client has to know size in bytes and data movements
// are purely by byte count
class buffer: public mem {
public:
	// Init
	// Set up a buffer based upon required capacity
	virtual void Init(
		const cl_command_queue	&cq,	// Specifies the device
		const size_t			&bytes);// required size in bytes at the time of creation

	// CopyTo
	// Copies specified byte count data to device buffer from host buffer.
	// The copy is blocking, i.e. guaranteed complete on return.
	virtual result CopyTo(
		const void		*host_buffer, 	// host's buffer as source
		const size_t	&bytes);		// size in bytes to copy
						
	// CopyFrom
	// Copies specified byte count data to host buffer from device buffer.
	// The copy is blocking, i.e. guaranteed complete on return.
	virtual result CopyFrom(									
		const size_t	&bytes,	 		// size in bytes to copy
		      void		*host_buffer);	// host's buffer as destination	

private:
	size_t bytes_;						// size in bytes

	// Init
	// Null Init() makes this class instantiable, overriding
	// the abstract declaration in mem
	virtual void Init() {}
};

// plane
// A 2D read/write image buffer of pixels stored in fours
// linearly, i.e. four pixels adjacent on a row.
// Includes padding:
//  - final byte4 on each row is padded with zeroes if necessary
//	- width and height of the buffer is further expanded and 
//    zero-padded to work-around an underlying CAL bug
class plane: public mem {
public:
	// Init
	// Set up a plane based upon pixel dimensions.
	//
	// Width and height are constrained. These constraints
	// can be used to ensure that the buffer size is a
	// multiple of the access sizes used by a kernel. e.g.
	// a kernel that accesses pixels in horizontal strips
	// of 4 (uchar4, effectively) can specify a 
	// width_constraint of 2.
	virtual void Init(
		const cl_command_queue	&cq,					// command queue, corresponds with the device holding the buffer
		const int				&width,					// width in pixels
		const int				&height,				// height in pixels
		const int				&width_constraint,		// power of 2 specifier for width of buffer
		const int				&height_constraint);	// power of 2 specifier for height of buffer

	// CopyTo
	// Copy pixels from host buffer to device buffer
	// Method returns immediately, i.e. copy completion 
	// is not guaranteed upon return
	result CopyTo(
		const	unsigned char	&host_buffer, 	// host's buffer of pixels in row major layout	
		const	int				&host_cols,	 	// count of pixels per row to be copied
		const	int				&host_rows,	 	// count of rows to be copied
		const	int				&host_pitch);	// size in pixels of each row of host buffer
					
	// CopyToAsynch
	// Copy pixels from host buffer to device buffer.
	// Method returns immediately, i.e. copy completion 
	// is not guaranteed upon return. Event can be used
	// to discern when copy has finished.
	result CopyToAsynch(
		const	unsigned char	&host_buffer, 	// host's buffer of pixels in row major layout	
		const	int				&host_cols,	 	// count of pixels per row to be copied		
		const	int				&host_rows,	 	// count of rows to be copied		
		const	int				&host_pitch,	// size in pixels of each row of host buffer
				cl_event		*event);		// event to track completion of this copy

	// CopyFrom
	// Copy pixels from device buffer to host buffer.
	// Method returns immediately, i.e. copy completion 
	// is not guaranteed upon return
	result CopyFrom(									
		const	int				&host_cols,	 	// count of pixels per row to be copied		
		const	int				&host_rows,	 	// count of rows to be copied						
		const	int				&host_pitch,	// size in pixels of each row of host buffer					
				unsigned char	*host_buffer);	// host's buffer of floats in row major layout		

	// CopyFromAsynch
	// Copy pixels from device buffer to host buffer.
	// Method returns immediately, i.e. copy completion 
	// is not guaranteed upon return. 
	// 
	// This copy will not start until the antecedent event's 
	// completion.
	//
	// Event can be used to discern when copy has finished.
	result CopyFromAsynch(
		const	int				&host_cols,	 	// count of pixels per row to be copied		
		const	int				&host_rows,	 	// count of rows to be copied							
		const	int				&host_pitch,	// size in pixels of each row of host buffer	
		const	cl_event		*antecedent,	// event that must complete before this copy can start
				cl_event		*event,			// event to track completion of this copy
				unsigned char	*host_buffer);	// host's buffer of floats in row major layout

protected:
	int		width_;		// width of plane buffer in pixels
	int		height_;	// height of plane buffer

private:
	// Init
	// Null Init() makes this class instantiable, overriding
	// the abstract declaration in mem
	virtual void Init() {}
};

// GetFormatPixel
// Returns a structure containing the correct settings
// for a 2D buffer of pixels organised in 4s horizontally.
cl_image_format GetFormatPixel();

#endif // _BUFFER_H_
