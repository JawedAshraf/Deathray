/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.00
 *
 * Copyright 2011, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#include <math.h>
#include "buffer_map.h"
#include "CLutil.h" 

int buffer_map::NewIndex() {

	if (buffer_map_.size() != 0)
		return buffer_map_.rbegin()->first + 1;
	else
		return 1;
}

result buffer_map::Append(mem **new_mem, int *new_index) {

	pair<map<int, mem*>::iterator, bool> insertionStatus;

	*new_index = NewIndex();
	insertionStatus = buffer_map_.insert (pair<int, mem*> (*new_index, *new_mem));

	if (insertionStatus.second)
		return FILTER_OK; 
	else
		return FILTER_ERROR;
}

result buffer_map::AllocBuffer(
	const	cl_command_queue	&cq,		
	const	size_t				&bytes,		
			int					*new_index) {

	result status = FILTER_OK;

	buffer *new_float_buffer = new buffer;
	new_float_buffer->Init(cq, bytes);
	if (new_float_buffer->valid()) {
		mem* new_mem = new_float_buffer;
		status = Append(&new_mem, new_index);
		return status;
	} else {
		return FILTER_ERROR;
	}
}

result buffer_map::CopyToBuffer(
	const	int		&index,			
	const	void	*host_buffer, 	
	const	size_t	&bytes) {

	buffer *destination;
	destination = static_cast<buffer*>(buffer_map_[index]);
	return destination->CopyTo(host_buffer, bytes);
}

result buffer_map::CopyFromBuffer(
	const	int		&index,			
	const	size_t	&bytes,	 		
			void	*host_buffer) {

	buffer* source;
	source = static_cast<buffer*>(buffer_map_[index]);
	return source->CopyFrom(bytes, host_buffer);
}

result buffer_map::AllocPlane(
	const	cl_command_queue	&cq,	
	const	int					&width, 
	const	int					&height,
			int					*new_index) {

	result status = FILTER_OK;

	plane* new_plane = new plane;
	new_plane->Init(cq, width, height, 2, 0);
	if (new_plane->valid()) {
		mem* new_mem = new_plane;
		status = Append(&new_mem, new_index);
		return status;
	} else {
		return FILTER_ERROR;
	}
}

result buffer_map::CopyToPlane(
	const	int		&index,
	const	byte	&host_buffer, 			
	const	int		&host_cols,	 			
	const	int		&host_rows,	 			
	const	int		&host_pitch) {

	plane* destination;
	destination = static_cast<plane*>(buffer_map_[index]);
	return destination->CopyTo(host_buffer, host_cols, host_rows, host_pitch);
}

result buffer_map::CopyToPlaneAsynch(
	const	int			&index,
	const	byte		&host_buffer, 			
	const	int			&host_cols,	 			
	const	int			&host_rows,	 			
	const	int			&host_pitch,
			cl_event	*event) {

	plane* destination;
	destination = static_cast<plane*>(buffer_map_[index]);
	return destination->CopyToAsynch(host_buffer, host_cols, host_rows, host_pitch, event);
}

result buffer_map::CopyFromPlane(
	const	int		&index,
	const	int		&host_cols,	 			
	const	int		&host_rows,	 			
	const	int		&host_pitch,
			byte	*host_buffer) {

	plane* source;
	source = static_cast<plane*>(buffer_map_[index]);
	return source->CopyFrom(host_cols, host_rows, host_pitch, host_buffer);
}

result buffer_map::CopyFromPlaneAsynch(
	const	int			&index,
	const	int			&host_cols,	 			
	const	int			&host_rows,	 			
	const	int			&host_pitch,
	const	cl_event	*antecedent,
			cl_event	*event,
			byte		*host_buffer) {

	plane* source;
	source = static_cast<plane*>(buffer_map_[index]);
	return source->CopyFromAsynch(host_cols, host_rows, host_pitch, antecedent, event, host_buffer);
}

void buffer_map::Destroy(const int &index) { 
	if (! ValidIndex(index)) return;	
		
	// Check reference count
	cl_uint reference_count = 0;
	clGetMemObjectInfo(buffer_map_[index]->obj(),
					   CL_MEM_REFERENCE_COUNT,
					   sizeof(cl_uint),
					   &reference_count,
					   0);

	// Repeatedly release it to ensure it will be destroyed
	for (cl_uint i = 0; i < reference_count; ++i) {
		clReleaseMemObject(buffer_map_[index]->obj());
	}

	buffer_map_.erase(index);
}

void buffer_map::DestroyAll() {
	map<int, mem*>::iterator each_buffer;
	map<int, mem*>::iterator next_buffer;
	each_buffer = buffer_map_.begin();
	while (each_buffer != buffer_map_.end()) {
		next_buffer = each_buffer;
		++next_buffer;
		Destroy(each_buffer->first);
		each_buffer = next_buffer;
	}
	buffer_map_.clear();
}

bool buffer_map::ValidIndex(const int &index) {
	return buffer_map_.count(index) > 0;
}

cl_mem* buffer_map::ptr(const int &index) {
	return buffer_map_[index]->ptr();
}
