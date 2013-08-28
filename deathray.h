/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.01
 *
 * Copyright 2013, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#ifndef _DEATHRAY_
#define _DEATHRAY_

#include "avisynth.h"
#include "result.h"
#include "buffer_map.h"

class deathray : public GenericVideoFilter {
public:
	deathray(PClip _child, double h_Y, double h_UV, int t_Y, int t_UV, double sigma, int sample_expand, IScriptEnvironment* env);

	~deathray(){};

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

private:
	// Init
	// Verifies that OpenCL is ready to go and that at least
	// one device is ready.
	result Init();

	// SetupFilters
	// Configure the global classes for single frame and multi
	// frame filtering.
	result SetupFilters();

	// InitPointers
	// Get the pointers for single frame filtering
	void InitPointers();
	
	// InitDimensions
	// Get the dimensions for filtering
	void InitDimensions();

	// PassThroughLuma
	// Puts unfiltered luma in destination
	void PassThroughLuma();
	
	// PassThroughChroma
	// Puts unfiltered chroma in destination
	void PassThroughChroma();

	// SingleFrameInit
	// Configure the plane-specific objects
	// for single frame filtering
	result SingleFrameInit();

	// SingleFrameExecute
	// Filter a single plane for any combination
	// of Y, U and V
	void SingleFrameExecute();

	// MultiFrameInit
	// Configure the plane-type specific objects
	// for multi frame filtering
	result MultiFrameInit();

	// MultiFrameCopy
	// Queries each plane type for the frame numbers
	// it requires, then provides the relevant host pointers
	// and activates the copy of host data to the device.
	void MultiFrameCopy(const int &n);

	// MultiFrameExecute
	// Filter a single plane for any combination
	// of Y, U and V over the entire temporal range.
	void MultiFrameExecute(const int &n);

	float h_Y_				;	// strength of luma noise reduction
	float h_UV_				;	// strength of chroma noise reduction
	int temporal_radius_Y_	;	// luma temporal radius
	int temporal_radius_UV_	;	// chroma temporal radius
	float sigma_			;	// gaussian weights are computed based upon sigma
	int sample_expand_		;	// factor by which the sample radius is expanded, e.g. 2 means sample radius of 6, since kernel has radius 3

	// Following are standard Avisynth properties of environment, source and destination: frames and planes
	IScriptEnvironment *env_;

	PVideoFrame src_;
	PVideoFrame dst_;

	const unsigned char *srcpY_;
	const unsigned char *srcpU_;
	const unsigned char *srcpV_;

	unsigned char *dstpY_;
    unsigned char *dstpU_;
    unsigned char *dstpV_;    

    int src_pitchY_;
    int src_pitchUV_;

	int dst_pitchY_;
    int dst_pitchUV_;
                    
    int row_sizeY_; 
    int row_sizeUV_; 
                  
    int heightY_;
    int heightUV_;

};

#endif