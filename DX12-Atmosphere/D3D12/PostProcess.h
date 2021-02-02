#pragma once
#include "stdafx.h"

class ColorBuffer;

namespace PostProcess
{
	struct ExposureConstants
	{
		float targetLuminance;
		float adaptationRate;
		float minExposure;
		float maxExposure;
		uint32_t pixelCount;
	};

	extern bool EnableHDR;

	extern bool EnableAdaptation;  // Automatically adjust brightness based on perceived luminance
	extern float Exposure;

	extern bool DrawHistogram;

	extern ExposureConstants ExposureCB;

	extern ColorBuffer HistogramColorBuffer;

	void Initialize(ColorBuffer* sceneBuffer);
	void Render();
	void UpdateUI(bool* showUI);
	void Shutdown();
}