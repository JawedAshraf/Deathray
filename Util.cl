/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.01
 *
 * Copyright 2013, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#define TILE_SIDE 53

__kernel void Zero(__global float4 *A) {
    int pos = get_global_id(0);
	A[pos] = 0.f;
}

// fast_power
// OpenCL on AMD GPUs doesn't expose the fast
// pow. The OpenCL pow function and its variants
// are all maximum precision. It's also more than
// twice as slow as this, which will have 1 or 2 ULP
// worse precision, I guess (not measured)
float fast_power(float x, float y) {
	return exp2(y * log2(x));
}

float4 fast_power4(float4 x, float y) {
	float4 power;

	float4 log_2 = log2(x);
#if 0 // slowest
	power.x = exp2(y * log2(x.x));
	power.y = exp2(y * log2(x.y));
	power.z = exp2(y * log2(x.z));
	power.w = exp2(y * log2(x.w));
#else
#if 0 // faster
	power = exp2(y * log_2);
#else // fastest
	power.x = exp2(y * log_2.x);
	power.y = exp2(y * log_2.y);
	power.z = exp2(y * log_2.z);
	power.w = exp2(y * log_2.w);
#endif
#endif

	return power;
}

// gamma_decode
// Converts input from gamma space into linear space.
float gamma_decode(float x) {
#if 1
	if (x <= 0.081f) return x/4.5f;
	return fast_power(((x + 0.099f)/1.099f), 2.22222222f);
#else
	return fast_power(x, 2.22222222f);
#endif
}

float4 gamma_decode4(float4 x) {
	float4 de_gamma;

	de_gamma.x = gamma_decode(x.x);
	de_gamma.y = gamma_decode(x.y);
	de_gamma.z = gamma_decode(x.z);
	de_gamma.w = gamma_decode(x.w);

	return de_gamma;
}

// gamma_encode
// Converts input from linear space into gamma space.
float gamma_encode(float x) {
#if 1
	if (x <= 0.018f) return 4.5f * x;
	return 1.099f * fast_power(x, 0.45f) - 0.099f;
#else
	return fast_power(x, 0.45f);
#endif
}

float4 gamma_encode4(float4 x) {
	float4 en_gamma;

	en_gamma.x = gamma_encode(x.x);
	en_gamma.y = gamma_encode(x.y);
	en_gamma.z = gamma_encode(x.z);
	en_gamma.w = gamma_encode(x.w);

	return en_gamma;
}

void WriteTile4(
	float4 store,
	int x,					// coordinates of a strip ...
	int y,					// ... of 4 pixels to be written
	local float *tile) {	// 48x48 portion of plane in local memory

	vstore4(store, 0, tile + ((y * TILE_SIDE) + x));
	write_mem_fence(CLK_LOCAL_MEM_FENCE);		
}

float4 ReadTile4(
	int x,					// coordinates of a strip ...
	int y,					// ... of 4 pixels to be fetched
	local float *tile) {	// 48x48 portion of plane in local memory

	float4 result;

	result = vload4(0, tile + ((y * TILE_SIDE) + x));
	read_mem_fence(CLK_LOCAL_MEM_FENCE);
	return result;
}

float16 ReadTile16(
	int x,					// coordinates of a strip ...
	int y,					// ... of 16 pixels to be fetched
	local float *tile) {	// 48x48 portion of plane in local memory

	float16 result;

	result = vload16(0, tile + ((y * TILE_SIDE) + x));
	read_mem_fence(CLK_LOCAL_MEM_FENCE);
	return result;
}

float16 Unpack10PixelsAndNormalise(uint4 sixteen_pixels) {
	// Unpack 10 pixels, which are packed into 128 bits as uchars,
	// into 10 normalised pixels as floats. 
	//
	// Only 10 pixels are required from each row of a window.

	uchar4 four_pixels;
	float16 result;

	four_pixels = as_uchar4(sixteen_pixels.x);

	result.s0 = (float)four_pixels.x * 0.0039215686f;
	result.s1 = (float)four_pixels.y * 0.0039215686f; 
	result.s2 = (float)four_pixels.z * 0.0039215686f; 
	result.s3 = (float)four_pixels.w * 0.0039215686f; 

	four_pixels = as_uchar4(sixteen_pixels.y);

	result.s4 = (float)four_pixels.x * 0.0039215686f; 
	result.s5 = (float)four_pixels.y * 0.0039215686f; 
	result.s6 = (float)four_pixels.z * 0.0039215686f; 
	result.s7 = (float)four_pixels.w * 0.0039215686f; 
                                
	four_pixels = as_uchar4(sixteen_pixels.z);

	result.s8 = (float)four_pixels.x * 0.0039215686f; 
	result.s9 = (float)four_pixels.y * 0.0039215686f; 
                                
	return result;
}

uint4 Pack10PixelsIntoTargetRow(float16 sixteen_pixels) {
	// Pack 10 float pixels into 10 uchars, stored as a uint4.
	//
	// Only 10 pixels are required for target window's row.

	uchar4 four_pixels;
	uint4 result;

	four_pixels.x = (uchar)(sixteen_pixels.s0 * 255.f);
	four_pixels.y = (uchar)(sixteen_pixels.s1 * 255.f);
	four_pixels.z = (uchar)(sixteen_pixels.s2 * 255.f);
	four_pixels.w = (uchar)(sixteen_pixels.s3 * 255.f);

	result.x = as_uint(four_pixels);
   
	four_pixels.x = (uchar)(sixteen_pixels.s4 * 255.f);
	four_pixels.y = (uchar)(sixteen_pixels.s5 * 255.f);
	four_pixels.z = (uchar)(sixteen_pixels.s6 * 255.f);
	four_pixels.w = (uchar)(sixteen_pixels.s7 * 255.f);
 
	result.y = as_uint(four_pixels);
                                      
	four_pixels.x = (uchar)(sixteen_pixels.s8 * 255.f);
	four_pixels.y = (uchar)(sixteen_pixels.s9 * 255.f);
 
	result.z = as_uint(four_pixels);
                         
	return result;
}

void Coordinates32x32(
	int2 *local_id,	// Work item
	int2 *source) {	// Each work item processes 4 pixels from source as a single row uchar4/float4

	// Generate coordinates for use by a variety of kernels based upon
	// a tile size of 32x32 pixels.

	// The 32x32 tile's top-left position in the plane, in uchar4s
	int2 tile_top_left = (int2)(get_group_id(0) << 3, get_group_id(1) << 5);

	*local_id = (int2)(get_local_id(0), get_local_id(1));
	*source = tile_top_left + *local_id; 
}

void FetchAndMirror48x48(
	read_only 	image2d_t 	target_plane,	// input image
	const		int			width,			// width in pixels
	const		int			height,			// height in pixels
	const		int2		local_id,		// Work item
	const		int2		source,			// coordinates
	local		float		*tile) {		// existing block of local memory populated with 48x48 pixels
	// Fetch pixels from source and populate a 48x48 tile of pixels.
	//
	// The tile is defined as a 32x32 region to be filtered plus an 
	// apron of 8 pixels on all four sides, making 48x48.
	//
	// The apron is filled with pixels from adjacent regions. At the
	// four edges of the frame the apron is filled, instead, with 
	// pixels mirrored from just inside the frame.

	const sampler_t plane = CLK_NORMALIZED_COORDS_FALSE |
							CLK_ADDRESS_CLAMP |
							CLK_FILTER_NEAREST;

	float4 source_pixels = read_imagef(target_plane, plane, source);

	write_mem_fence(CLK_LOCAL_MEM_FENCE);		

	// Scalar address for left-most pixel of 4 pixels that are put into tile
	int2 target = (int2)((local_id.x << 2) + 8, local_id.y + 8);

	WriteTile4(source_pixels, target.x, target.y, tile);
	
	if (local_id.y < 8 || local_id.y > 23) { // 8 top and bottom rows of apron are filled from adjacent tiles
		int offset = (local_id.y < 8) ? -8 : 8;
		source_pixels = read_imagef(target_plane, plane, source + (int2)(0, offset));
		WriteTile4(source_pixels, target.x, target.y + offset, tile); 
	}	
	if (local_id.x < 2 || local_id.x > 5) { // 8 columns at left and right
		int offset = (local_id.x < 2) ? -2 : 2;
		source_pixels = read_imagef(target_plane, plane, source + (int2)(offset, 0));
		WriteTile4(source_pixels, target.x + (offset << 2), target.y, tile); 
	}	
	if (local_id.y < 8 || local_id.y > 23) { // 4 corners, each 8x8
		int2 offset;
		offset.y = (local_id.y < 8) ? -8 : 8;
		if (local_id.x < 2 || local_id.x > 5) {
			offset.x = (local_id.x < 2) ? -2 : 2;
			source_pixels = read_imagef(target_plane, plane, source + offset);
			WriteTile4(source_pixels, target.x + (offset.x << 2), target.y + offset.y, tile);
		}
	}
	barrier(CLK_LOCAL_MEM_FENCE);

	// Mirror into apron when at frame border
	float4 mirror;
	bool left_group  = (get_group_id(0) == 0);
	bool right_group = (get_group_id(0) == (get_num_groups(0) - 1));
	bool top_group   = (get_group_id(1) == 0);
	bool bot_group   = (get_group_id(1) == (get_num_groups(1) - 1));

	if (top_group && local_id.y < 8) { 	// top 8 rows
		mirror = ReadTile4(target.x, target.y + 1, tile);
		WriteTile4(mirror, target.x, 8 - local_id.y - 1, tile);
	}
	barrier(CLK_LOCAL_MEM_FENCE);
	
	if (bot_group && local_id.y < 8) { // bottom 8 rows
		int centre = ((height - 1) & 31) + 8;

		mirror = ReadTile4(target.x, centre - local_id.y - 1, tile);
		WriteTile4(mirror, target.x, centre + local_id.y + 1, tile);
	}
	barrier(CLK_LOCAL_MEM_FENCE);
	// left 8 columns + top-left and bottom-left apron corners per tile
	int read_x  = (local_id.x == 1) ? 13 : 9;
	int write_x = (local_id.x == 1) ? 0 : 4;
	if (local_id.x < 2) {
		if (left_group) { // left 8 columns, 32 rows
			mirror = ReadTile4(read_x, target.y, tile);
			WriteTile4(mirror.wzyx, write_x, target.y, tile);
		}

		// TODO - cleanup IF structure - not sure if the compiler will jinx this
		if (local_id.y < 8) { 	
			if (left_group) { // left edge of frame apron corners, top and bottom
				mirror = ReadTile4(read_x, local_id.y, tile);
				WriteTile4(mirror.wzyx, write_x, local_id.y, tile);
				mirror = ReadTile4(read_x, 40 + local_id.y, tile);
				WriteTile4(mirror.wzyx, write_x, 40 + local_id.y, tile);
			}
		}
	}
	barrier(CLK_LOCAL_MEM_FENCE);
	if (local_id.x < 2 && local_id.y < 8) {
		int x = (local_id.x == 1) ? 4 : 0;
		if (top_group) { // apron at top edge of frame, left corners
			mirror = ReadTile4(x, 8 + local_id.y + 1, tile);
			WriteTile4(mirror, x, 8 - local_id.y - 1, tile);
		}
		if (bot_group) { // apron at bottom edge of frame, left corners
			int bottom = ((height - 1) & 31) + 8;
			mirror = ReadTile4(x, bottom - local_id.y - 1, tile);
			WriteTile4(mirror, x, bottom + local_id.y + 1, tile);
		}
	}
	barrier(CLK_LOCAL_MEM_FENCE);

	if (local_id.x > 5) { // right 8 columns + top-right and bottom-right apron corners per tile
		
		if (right_group) { // right edge of frame 
			int centre  = ((width - 1) & 31) + 8;
			int read_x  = (local_id.x == 7) ? centre - 4 : centre - 8;
			int write_x = (local_id.x == 7) ? centre + 1 : centre + 5;

			mirror = ReadTile4(read_x, target.y, tile);
			WriteTile4(mirror.wzyx, write_x, target.y, tile); 

			if (local_id.y < 8) { // top right and bottom right corner of every tile on right edge				
				mirror = ReadTile4(read_x, local_id.y, tile);
				WriteTile4(mirror.wzyx, write_x, local_id.y, tile);
				mirror = ReadTile4(read_x, 40 + local_id.y, tile);
				WriteTile4(mirror.wzyx, write_x, 40 + local_id.y, tile);
					
				if (bot_group) { // bottom right corner of frame
					int bottom = ((height - 1) & 31) + 8;
					mirror = ReadTile4(read_x, bottom + local_id.y + 1, tile);
					WriteTile4(mirror.wzyx, write_x, bottom + local_id.y + 1, tile);
				}
			}
		} else if ((top_group || bot_group) && local_id.y < 8) {
			if (top_group) {
				int x = (local_id.x == 7) ? 44 : 40; // ATI 2.3: compiler bug requires this here if TILE_SIDE is not 48
				mirror = ReadTile4(x, 8 + local_id.y + 1, tile);
				WriteTile4(mirror, x, 8 - local_id.y - 1, tile);
			}
			if (bot_group) {
				int x = (local_id.x == 7) ? 44 : 40; // ATI 2.3: compiler bug requires this here if TILE_SIDE is not 48
				int bottom = ((height - 1) & 31) + 8;
				mirror = ReadTile4(x, bottom - local_id.y - 1, tile);
				WriteTile4(mirror, x, bottom + local_id.y + 1, tile);
			}
		}
	}
	barrier(CLK_LOCAL_MEM_FENCE);
}