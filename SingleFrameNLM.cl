/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.04
 *
 * Copyright 2013, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

__attribute__((reqd_work_group_size(8, 32, 1)))
__kernel void NLMSingleFrameFourPixel(
	read_only 	image2d_t 	target_plane,			// input plane
	const		int			width,					// width in pixels
	const		int			height,					// height in pixels
	const		float		h,						// strength of denoising
	const		int			sample_expand,			// factor to expand sample radius
	constant	float		*g_gaussian,			// 49 weights of guassian kernel
	const		int			linear,					// process plane in linear space instead of gamma space
	const		int			correction,				// apply a post-filtering correction
	const		int			target_min,				// target pixel is weighted using minimum weight of samples, not maximum
	const		int			balanced,				// balanced tonal range de-noising
	write_only 	image2d_t 	destination_plane) {	// filtered result
	// Each work group produces 1024 filtered pixels, organised as a tile
	// of 32x32.
	//
	// Each work item computes 4 pixels in a contiguous horizontal strip.
	//
	// The tile is bordered with an apron that's 8 pixels wide. This apron
	// consists of pixels from adjoining tiles, where available. If the
	// tile is at the frame edge, the apron is filled with pixels mirrored
	// from just inside the frame.
	//
	// Input plane contains pixels as uchars. UNORM8 format is defined,
	// so a read converts uchar into a normalised float of range 0.f to 1.f. 
	// 
	// Destination plane is formatted as UNORM8 uchar. The device 
	// automatically converts a pixel in range 0.f to 1.f into 0 to 255.

	__local float target_tile[TILE_SIDE * TILE_SIDE];

	int2 local_id;
	int2 source;
	Coordinates32x32(&local_id, &source);

	float4 average = 0.f;
	float4 weight = 0.f;
	float4 target_weight = target_min ? MAXFLOAT : 0.f;
	float4 filtered_pixels;

	// Inside local memory the top-left corner of the tile is at (8,8)
	int2 target = (int2)((local_id.x << 2) + 8, local_id.y + 8);

	// The tile is 48x48 pixels which is entirely filled from the source
	FetchAndMirror48x48(target_plane, width, height, local_id, source, linear, target_tile) ;

	int kernel_radius = 3;
	float16 target_window[7];
	for (int y = 0; y < 2 * kernel_radius + 1; ++y) {
		target_window[y] = ReadTile16(target.x - kernel_radius,
									  target.y + y - kernel_radius, 
									  target_tile);
	}

	Filter4(target,	h, sample_expand, target_window, target_tile, g_gaussian, 1, target_min, balanced, &average, &weight, &target_weight);
	filtered_pixels = average / weight;
	if (correction) {
		float4 original = ReadPixel4(target_plane, source, linear);

		float4 difference = filtered_pixels - original;
		float4 correction = (difference * original * original) - 
							((difference * original) * (difference * original));

		filtered_pixels = filtered_pixels - correction;
	}
	WritePixel4(filtered_pixels, source, linear, destination_plane);
}
