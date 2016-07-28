/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.04
 *
 * Copyright 2013, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */

#include "util.h"

// October 2010:
// 14 is good enough for D3D11 devices, but after 6 months+ of this bug
// there's no telling how soon it will disappear, or what the maximum
// texture dimension will be by then. So, 16 allows some margin
// January 2011:
// Status of this bug has not been checked, since SDK 2.3 and Catalyst 10.12
// were released. Soon. Promise...
#define MAX_TEXTURE_DIMENSION_AS_POWER_OF_2 16

int ByPowerOf2(unsigned int x, int power_of_2) {
	int p = (power_of_2 > 30) ? 30 : power_of_2 ;

	return (((x ? x : 1) + (1 << p) - 1) >> p ) << p ;
}

int FixCALBufferSizeFault(const int& length) {
	// Rule is simply that {0, 1, 2, 3} * PowerOf2
	// is the only valid length of a 2D buffer's dimensions
	int rescale = 0 ;

	// TODO should be faster in reverse, but needs careful testing
	for (int i = 3; i <= MAX_TEXTURE_DIMENSION_AS_POWER_OF_2; ++i) {
		if (length >> i) {
			rescale = i - 2 ;
		}
	}

	return ByPowerOf2 (length, rescale) ;
}

void GetFrameDimensions(
	const int &width,				
	const int &height,				
	const int &width_power_of_2,	
	const int &height_power_of_2,	
		  int *device_width,			
		  int *device_height) {

	const int checked_width_power_of_2 = (width_power_of_2 < 2) ? 2 : width_power_of_2;
	const int element_count = ByPowerOf2(width, 2) >> 2;
	const int element_width = ByPowerOf2(element_count, checked_width_power_of_2 - 2);
	const int element_height = ByPowerOf2(height, height_power_of_2);

	*device_width = FixCALBufferSizeFault(element_width);
	*device_height = FixCALBufferSizeFault(element_height);
}

result GetSourceFromResource(int resource_id, string *source) {
	// resource.h contains a set of #DEFINEs that specify
	// the "filenames" of resources that have been compiled
	// into the DLL.
	// When a resource is compiled into the DLL it does not
	// have a trailing \0. So the resource length is used
	// to ensure correct extraction, as LockResource
	// simply returns characters until it finds a \0.    

	HMODULE		dll_handle; 
    HRSRC       kernel_text_resource;
    HGLOBAL     kernel_text_resource_handle = NULL;
    char*	    kernel_text;
    DWORD       kernel_text_size;

	wchar_t*	dll_name = L"deathray.dll";
	if ((dll_handle = GetModuleHandle(dll_name)) == NULL) {
		return FILTER_DLL_NOT_FOUND;
	}

    kernel_text_resource = FindResource(dll_handle, MAKEINTRESOURCE(resource_id), RT_RCDATA);
	if (!kernel_text_resource) {
        return FILTER_ERROR;
	}

    kernel_text_resource_handle = LoadResource(dll_handle, kernel_text_resource);
	if (!kernel_text_resource_handle) {
        return FILTER_ERROR;
	}

	kernel_text = (char*)LockResource(kernel_text_resource_handle);
    kernel_text_size = SizeofResource(dll_handle, kernel_text_resource);

	// Truncate the text correctly (random crap until a \0 appears, otherwise)
	source->assign(kernel_text, kernel_text_size) ;

	return FILTER_OK ; 
}
