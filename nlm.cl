/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.00
 *
 * Copyright 2011, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

void Filter4(
	const		int2	target,					// tile coordinates of left-hand pixel of 4 pixels in a horizontal strip
	const		float	h,						// strength of denoising
	const		int		sample_expand,			// factor to expand sample radius
				float16	*target_window,			// a window of 10x7 pixels, centred upon the 4 pixels being filtered
	local		float	*sample_tile,			// factor to expand sample radius
	constant	float	*gaussian,				// 49 weights of guassian kernel
	const		int		reweight_target_pixel,	// when target plane is the sampling plane, the target pixel is reweighted
				float4	*all_samples_average,	// running sum of weighted pixel values
				float4	*all_samples_weight) {	// running sum of weights

	// Computes the gaussian-weighted average of the target pixels' windows
	// against all sample windows from the tile.
	//
	// The sample windows nominally number 49, but sample_expand can be used
	// to increase the sampling radius. This introduces errors when 2 or more
	// but the errors are hard to see unless strong sigma and/or h values 
	// are supplied. Error increases with sample_expand, producing visible
	// edges corresponding with the tile borders within the plane.

	int kernel_radius = 3;
	int sample_radius = kernel_radius * sample_expand;

	int2 sample_start = max(target - sample_radius, 3);
	int2 sample_end	  = {min(target.x + sample_radius, 41), min(target.y + sample_radius, 44)};

	float4 sample_centre_pixel;
	float4 weight_max = 0.f;
	int2 sample;
	for (sample.y = sample_start.y; sample.y <= sample_end.y; ++sample.y) {
		for (sample.x = sample_start.x; sample.x <= sample_end.x; ++sample.x) {
			int gaussian_position = 0;
			float4 euclidean_distance = 0;
			int target_row = 0;
			for (int y = -kernel_radius; y < kernel_radius + 1; ++y, ++target_row) {
				float16 sample_window_row = ReadTile16(sample.x - kernel_radius,
													   sample.y + y, 
													   sample_tile);

				float4 diff = target_window[target_row].s0123 - sample_window_row.s0123;
				euclidean_distance += gaussian[gaussian_position] * (diff * diff);
				diff = target_window[target_row].s6789 - sample_window_row.s6789;
				euclidean_distance += gaussian[gaussian_position] * (diff * diff);

				diff = target_window[target_row].s1234 - sample_window_row.s1234;
				euclidean_distance += gaussian[gaussian_position + 1] * (diff * diff);
				diff = target_window[target_row].s5678 - sample_window_row.s5678;
				euclidean_distance += gaussian[gaussian_position + 1] * (diff * diff);

				diff = target_window[target_row].s2345 - sample_window_row.s2345;
				euclidean_distance += gaussian[gaussian_position + 2] * (diff * diff);
				diff = target_window[target_row].s4567 - sample_window_row.s4567;
				euclidean_distance += gaussian[gaussian_position + 2] * (diff * diff);

				diff = target_window[target_row].s3456 - sample_window_row.s3456;
				euclidean_distance += gaussian[gaussian_position + 3] * (diff * diff);

				gaussian_position += 7;
			}

			float4 sample_weight = exp(-euclidean_distance / h);	

			weight_max = max(weight_max, sample_weight);
			
			sample_weight = (sample.x == target.x && sample.y == target.y && reweight_target_pixel) 
						  ? 0.f 
						  : sample_weight;

			*all_samples_weight += sample_weight;

			sample_centre_pixel = ReadTile4(sample.x, sample.y, sample_tile);
			*all_samples_average += sample_weight * sample_centre_pixel;
		}
	}

	*all_samples_weight += reweight_target_pixel ? weight_max : 0.f;

	sample_centre_pixel = ReadTile4(target.x, target.y, sample_tile);
	*all_samples_average +=  reweight_target_pixel ? weight_max * sample_centre_pixel : 0.f;
}
