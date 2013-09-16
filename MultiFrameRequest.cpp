/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.02
 *
 * Copyright 2013, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#include "MultiFrameRequest.h"

void MultiFrameRequest::Request(int frame_number) {
	frames_.insert(pair<int, const unsigned char*>(frame_number, static_cast<const unsigned char*>(NULL)));
}
bool MultiFrameRequest::GetFrameNumber(int *frame_number) {
	map<int, const unsigned char*>::iterator find_frame;
	
	for (find_frame = frames_.begin(); find_frame != frames_.end(); ++find_frame) {
		if (find_frame->second == NULL) {
			*frame_number = find_frame->first;
			return true;
		}
	}

	return false;
}
void MultiFrameRequest::Supply(int frame_number, const unsigned char* host_pointer) {
	frames_[frame_number] = host_pointer;
}
const unsigned char* MultiFrameRequest::Retrieve(int frame_number) {
	return frames_[frame_number];
}
