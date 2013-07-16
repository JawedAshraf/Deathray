/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.00
 *
 * Copyright 2011, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#ifndef MULTI_FRAME_REQUEST_H_
#define MULTI_FRAME_REQUEST_H_

#include <map>
using namespace std;

// MultiFrameRequest
// Holds requests for the frames that are required in multi-frame processing.
// Luma and chroma processing can each independently request a set of frames
// that Avisynth should provide.
class MultiFrameRequest
{
public:

	MultiFrameRequest() {}
	~MultiFrameRequest() {}

	// Request
	// Add a request for the specified frame number.
	void Request(int frame_number);

	// GetFrameNumber
	// Get the first frame number that has a null
	// pointer that needs to be supplied.
	// Return value is true if a frame number 
	// can be supplied, otherwise all frame
	// numbers have non-null pointers.
	bool GetFrameNumber(int *frame_number);

	// Supply
	// Supply a pointer for the associated frame number.
	void Supply(
		int						frame_number, 
		const unsigned char		*host_pointer);

	// Retrieve
	// Retrieve the pointer associated with the frame number.
	const unsigned char* Retrieve(int frame_number);

private:

	map<int, const unsigned char*>	frames_;
};

#endif // MULTI_FRAME_REQUEST_H_