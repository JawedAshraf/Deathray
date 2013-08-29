/* Deathray - An Avisynth plug-in filter for spatial/temporal non-local means de-noising.
 *
 * version 1.01
 *
 * Copyright 2013, Jawed Ashraf - Deathray@cupidity.f9.co.uk
 */


#include <windows.h>
#include <math.h>
#include "clutil.h"
#include "device.h"
#include "deathray.h"
#include "SingleFrame.h"
#include "MultiFrame.h"
#include "MultiFrameRequest.h"

device	*g_devices		= NULL;
int		g_device_count	= 0;

bool	g_opencl_available = false;
bool	g_opencl_failed_to_initialise = false;

// Buffer containing the gaussian weights
int g_gaussian = 0;

SingleFrame g_SingleFrame_Y;
SingleFrame g_SingleFrame_U;
SingleFrame g_SingleFrame_V;

MultiFrame g_MultiFrame_Y; 
MultiFrame g_MultiFrame_U;
MultiFrame g_MultiFrame_V;

void GaussianGenerator(float sigma) {
	float two_sigma_squared = 2 * sigma * sigma;

	float gaussian[49]; 
	float gaussian_sum = 0;

	for (int y = -3; y < 4; ++y) {
		for (int x = -3; x < 4; ++x) {
			int index = 7 * (y + 3) + x + 3;
			gaussian[index] = exp(-(x * x + y * y) / two_sigma_squared) / (3.14159265f * two_sigma_squared);
			gaussian_sum += gaussian[index];
		}
	}

	for (int i = 0; i < 49; ++i)
		gaussian[i] /= gaussian_sum;

	g_devices[0].buffers_.AllocBuffer(g_devices[0].cq(), 49 * sizeof(float), &g_gaussian);
	g_devices[0].buffers_.CopyToBuffer(g_gaussian, gaussian, 49 * sizeof(float));
}

deathray::deathray(PClip child, 
				   double h_Y, 
				   double h_UV, 
				   int temporal_radius_Y, 
				   int temporal_radius_UV,
				   double sigma,
				   int sample_expand, 
				   int linear,
				   IScriptEnvironment *env) :	GenericVideoFilter(child),
												h_Y_(static_cast<float>(h_Y/10000.)), 
												h_UV_(static_cast<float>(h_UV/10000.)), 
												temporal_radius_Y_(temporal_radius_Y),
												temporal_radius_UV_(temporal_radius_UV),
												sigma_(static_cast<float>(sigma)),
												sample_expand_(sample_expand),
												linear_(linear),
												env_(env){
}

result deathray::Init() {
	if (g_devices != NULL) return FILTER_OK;

	// No point continuing, as prior attempt failed
	if (g_opencl_failed_to_initialise) return FILTER_ERROR;

	int device_count = 0;
	result status = StartOpenCL(&device_count);
	if (device_count != 0) {
		g_opencl_available = true;
		GaussianGenerator(sigma_);
		status = SetupFilters();
	} else {
		g_opencl_failed_to_initialise = true;
	}

	return status;
}

result deathray::SetupFilters() {
	result status = FILTER_OK;

	if ((temporal_radius_Y_ == 0 && h_Y_ > 0.f) || (temporal_radius_UV_ == 0 && h_UV_ > 0.f)) {
		status = SingleFrameInit();
		if (status != FILTER_OK) env_->ThrowError("Single-frame initialisation failed, status=%d and OpenCL status=%d", status, g_last_cl_error);	
	}
	if ((temporal_radius_Y_ > 0 && h_Y_ > 0.f) || (temporal_radius_UV_ > 0 && h_UV_ > 0.f)) {
		status = MultiFrameInit();
		if (status != FILTER_OK) env_->ThrowError("Multi-frame initialisation failed, status=%d and OpenCL status=%d", status, g_last_cl_error);	
	}	

	return status;
}

PVideoFrame __stdcall deathray::GetFrame(int n, IScriptEnvironment *env) {
    src_ = child->GetFrame(n, env);
    dst_ = env->NewVideoFrame(vi);

	InitPointers();
	InitDimensions();

	if (h_Y_ == 0.f)					PassThroughLuma();
	if (h_UV_ == 0.f)					PassThroughChroma();
	if (h_Y_ == 0.f && h_UV_ == 0.f)	return dst_;

	result status = FILTER_OK;
	status = Init();
	if (status != FILTER_OK || !(vi.IsPlanar())) { 
		if (g_opencl_failed_to_initialise) {
			env->ThrowError("Deathray: Error in OpenCL status=%d frame %d and OpenCL status=%d", status, n, g_last_cl_error);
		} else {
			env->ThrowError("Deathray: Check that clip is planar format - status=%d frame %d", status, n);
		}
	}

	if ((temporal_radius_Y_ == 0 && h_Y_ > 0.f) || (temporal_radius_UV_ == 0 && h_UV_ > 0.f))
		SingleFrameExecute();

	if ((temporal_radius_Y_ > 0 && h_Y_ > 0.f) || (temporal_radius_UV_ > 0 && h_UV_ > 0.f))
		MultiFrameExecute(n);

	return dst_;
}

void deathray::InitPointers() {
    srcpY_ = src_->GetReadPtr(PLANAR_Y);
    srcpU_ = src_->GetReadPtr(PLANAR_U);
    srcpV_ = src_->GetReadPtr(PLANAR_V);    

    dstpY_ = dst_->GetWritePtr(PLANAR_Y);
    dstpU_ = dst_->GetWritePtr(PLANAR_U);
    dstpV_ = dst_->GetWritePtr(PLANAR_V);    
}

void deathray::InitDimensions() {
    src_pitchY_ = src_->GetPitch(PLANAR_Y);
    src_pitchUV_ = src_->GetPitch(PLANAR_V);

	dst_pitchY_ = dst_->GetPitch(PLANAR_Y);
    dst_pitchUV_ = dst_->GetPitch(PLANAR_V);
                
    row_sizeY_ = src_->GetRowSize(PLANAR_Y); 
    row_sizeUV_ = src_->GetRowSize(PLANAR_V); 
              
    heightY_ = src_->GetHeight(PLANAR_Y);
    heightUV_ = src_->GetHeight(PLANAR_V);
}

void deathray::PassThroughLuma() {
	env_->BitBlt(dstpY_, dst_pitchY_, srcpY_, src_pitchY_, row_sizeY_, heightY_);
}

void deathray::PassThroughChroma() {
	env_->BitBlt(dstpV_, dst_pitchUV_, srcpV_, src_pitchUV_, row_sizeUV_, heightUV_);
	env_->BitBlt(dstpU_, dst_pitchUV_, srcpU_, src_pitchUV_, row_sizeUV_, heightUV_);
}

result deathray::SingleFrameInit() {
	result status = FILTER_OK;
			
	if (temporal_radius_Y_ == 0 && h_Y_ > 0.f) {
		status = g_SingleFrame_Y.Init(0, row_sizeY_, heightY_, src_pitchY_, dst_pitchY_, h_Y_, sample_expand_, linear_);
		if (status != FILTER_OK) return status;
	}

	if (temporal_radius_UV_ == 0 && h_UV_ > 0.f) {
		status = g_SingleFrame_U.Init(0, row_sizeUV_, heightUV_, src_pitchUV_, dst_pitchUV_, h_UV_, sample_expand_, 0);
		if (status != FILTER_OK) return status;

		status = g_SingleFrame_V.Init(0, row_sizeUV_, heightUV_, src_pitchUV_, dst_pitchUV_, h_UV_, sample_expand_, 0);
		if (status != FILTER_OK) return status;
	}

	return status;
}

void deathray::SingleFrameExecute() {	
	cl_uint wait_list_length = 0;
	cl_event wait_list[3];
	result status;

	if (temporal_radius_Y_ == 0 && h_Y_ > 0.f) {
		status = g_SingleFrame_Y.CopyTo(srcpY_);
		if (status != FILTER_OK) env_->ThrowError("Deathray: Copy Y to device status=%d and OpenCL status=%d", status, g_last_cl_error);
	}
	if (temporal_radius_UV_ == 0 && h_UV_ > 0.f) {
		status = g_SingleFrame_U.CopyTo(srcpU_);
		if (status != FILTER_OK) env_->ThrowError("Deathray: Copy U to device status=%d and OpenCL status=%d", status, g_last_cl_error);
		status = g_SingleFrame_V.CopyTo(srcpV_);
		if (status != FILTER_OK) env_->ThrowError("Deathray: Copy V to device status=%d and OpenCL status=%d", status, g_last_cl_error);
	}

	if (temporal_radius_Y_ == 0 && h_Y_ > 0.f) {
		status = g_SingleFrame_Y.Execute();
		if (status != FILTER_OK) env_->ThrowError("Deathray: Execute Y kernel status=%d and OpenCL status=%d", status, g_last_cl_error);
		status = g_SingleFrame_Y.CopyFrom(dstpY_, wait_list);
		if (status != FILTER_OK) env_->ThrowError("Deathray: Copy Y to host status=%d and OpenCL status=%d", status, g_last_cl_error);
		++wait_list_length;
	}

	if (temporal_radius_UV_ == 0 && h_UV_ > 0.f) {
		g_SingleFrame_U.Execute();
		if (status != FILTER_OK) env_->ThrowError("Deathray: Execute U kernel status=%d and OpenCL status=%d", status, g_last_cl_error);
		g_SingleFrame_U.CopyFrom(dstpU_, wait_list + wait_list_length++);
		if (status != FILTER_OK) env_->ThrowError("Deathray: Copy U to host status=%d and OpenCL status=%d", status, g_last_cl_error);
		g_SingleFrame_V.Execute();
		if (status != FILTER_OK) env_->ThrowError("Deathray: Execute V kernel status=%d and OpenCL status=%d", status, g_last_cl_error);
		g_SingleFrame_V.CopyFrom(dstpV_, wait_list + wait_list_length++);
		if (status != FILTER_OK) env_->ThrowError("Deathray: Copy V to host status=%d and OpenCL status=%d", status, g_last_cl_error);
	}

	clWaitForEvents(wait_list_length, wait_list);
}

result deathray::MultiFrameInit() {
	result status = FILTER_OK;

	if (temporal_radius_Y_ > 0 && h_Y_ > 0.f) {
		status = g_MultiFrame_Y.Init(0, temporal_radius_Y_, row_sizeY_, heightY_, src_pitchY_, dst_pitchY_, h_Y_, sample_expand_, linear_);
		if (status != FILTER_OK) return status;
	}

	if (temporal_radius_UV_ > 0 && h_UV_ > 0.f) {
		status = g_MultiFrame_U.Init(0, temporal_radius_UV_, row_sizeUV_, heightUV_, src_pitchUV_, dst_pitchUV_, h_UV_, sample_expand_, 0);
		if (status != FILTER_OK) return status;

		status = g_MultiFrame_V.Init(0, temporal_radius_UV_, row_sizeUV_, heightUV_, src_pitchUV_, dst_pitchUV_, h_UV_, sample_expand_, 0);
		if (status != FILTER_OK) return status;
	}

	return status;
}

void deathray::MultiFrameCopy(const int &n) {
	result status = FILTER_OK;

	int frame_number;
	if (temporal_radius_Y_ > 0 && h_Y_ > 0.f) {
		MultiFrameRequest frames_Y;
		g_MultiFrame_Y.SupplyFrameNumbers(n, &frames_Y);
		while (frames_Y.GetFrameNumber(&frame_number)) {
			PVideoFrame Y = child->GetFrame(frame_number, env_);
			const unsigned char* ptr_Y = Y->GetReadPtr(PLANAR_Y);
			frames_Y.Supply(frame_number, ptr_Y);
		}
		status = g_MultiFrame_Y.CopyTo(&frames_Y);
		if (status != FILTER_OK ) env_->ThrowError("Deathray: Copy Y to device, status=%d and OpenCL status=%d", status, g_last_cl_error);
	}

	if (temporal_radius_UV_ > 0 && h_UV_ > 0.f) {
		MultiFrameRequest frames_U;
		MultiFrameRequest frames_V;
		g_MultiFrame_U.SupplyFrameNumbers(n, &frames_U);
		g_MultiFrame_V.SupplyFrameNumbers(n, &frames_V);
		while (frames_U.GetFrameNumber(&frame_number)) {
			PVideoFrame UV = child->GetFrame(frame_number, env_);
			const unsigned char* ptr_U = UV->GetReadPtr(PLANAR_U);
			const unsigned char* ptr_V = UV->GetReadPtr(PLANAR_V);
			frames_U.Supply(frame_number, ptr_U);
			frames_V.Supply(frame_number, ptr_V);
		}
		status = g_MultiFrame_U.CopyTo(&frames_U);
		if (status != FILTER_OK ) env_->ThrowError("Deathray: Copy U to device, status=%d and OpenCL status=%d", status, g_last_cl_error);
		status = g_MultiFrame_V.CopyTo(&frames_V);
		if (status != FILTER_OK ) env_->ThrowError("Deathray: Copy V to device, status=%d and OpenCL status=%d", status, g_last_cl_error);
	}
}

void deathray::MultiFrameExecute(const int &n) {
	cl_uint wait_list_length = 0;
	cl_event wait_list[3];
	result status = FILTER_OK;

	MultiFrameCopy(n);

	if (temporal_radius_Y_ > 0 && h_Y_ > 0.f) {
		status = g_MultiFrame_Y.Execute();
		if (status != FILTER_OK) env_->ThrowError("Deathray: Execute Y kernel status=%d and OpenCL status=%d", status, g_last_cl_error);
		status = g_MultiFrame_Y.CopyFrom(dstpY_, wait_list);
		if (status != FILTER_OK) env_->ThrowError("Deathray: Copy Y to host status=%d and OpenCL status=%d", status, g_last_cl_error);
		++wait_list_length;
	}

	if (temporal_radius_UV_ > 0 && h_UV_ > 0.f) {
		g_MultiFrame_U.Execute();
		if (status != FILTER_OK) env_->ThrowError("Deathray: Execute U kernel status=%d and OpenCL status=%d", status, g_last_cl_error);
		g_MultiFrame_U.CopyFrom(dstpU_, wait_list + wait_list_length++);
		if (status != FILTER_OK) env_->ThrowError("Deathray: Copy U to host status=%d and OpenCL status=%d", status, g_last_cl_error);
		g_MultiFrame_V.Execute();
		if (status != FILTER_OK) env_->ThrowError("Deathray: Execute V kernel status=%d and OpenCL status=%d", status, g_last_cl_error);
		g_MultiFrame_V.CopyFrom(dstpV_, wait_list + wait_list_length++);
		if (status != FILTER_OK) env_->ThrowError("Deathray: Copy V to host status=%d and OpenCL status=%d", status, g_last_cl_error);
	}

	clWaitForEvents(wait_list_length, wait_list);
}

AVSValue __cdecl CreateDeathray(AVSValue args, void *user_data, IScriptEnvironment *env) {

	double h_Y = args[1].AsFloat(1.);
	if (h_Y < 0.) h_Y = 0.;

	double h_UV = args[2].AsFloat(1.);
	if (h_UV < 0.) h_UV = 0.;

	int temporal_radius_Y = args[3].AsInt(0);
	if (temporal_radius_Y < 0) temporal_radius_Y = 0;
	if (temporal_radius_Y > 64) temporal_radius_Y = 64;

	int temporal_radius_UV = args[4].AsInt(0);
	if (temporal_radius_UV < 0) temporal_radius_UV = 0;
	if (temporal_radius_UV > 64) temporal_radius_UV = 64;

	double sigma = args[5].AsFloat(1.);
	if (sigma < 0.1) sigma = 0.1;

	int sample_expand = args[6].AsInt(1);	
	if (sample_expand <= 0) sample_expand = 1;
	if (sample_expand > 14) sample_expand = 14;

	int linear = args[7].AsBool(false) ? 1 : 0;

	return new deathray(args[0].AsClip(),
						h_Y, 
						h_UV, 
						temporal_radius_Y, 
						temporal_radius_UV, 
						sigma,
						sample_expand, 
						linear,
						env);
}

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit2(IScriptEnvironment *env) {
    env->AddFunction("deathray", "c[hY]f[hUV]f[tY]i[tUV]i[s]f[x]i[l]b", CreateDeathray, 0);

    return "Deathray";
}
