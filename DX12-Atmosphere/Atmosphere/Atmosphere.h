#pragma once
#include "stdafx.h"

class ColorBuffer;

namespace Atmosphere
{
	enum Luminance {
		// Render the spectral radiance at kLambdaR, kLambdaG, kLambdaB.
		NONE,
		// Render the sRGB luminance, using an approximate (on the fly) conversion
		// from 3 spectral radiance values only (see section 14.3 in <a href=
		// "https://arxiv.org/pdf/1612.04336.pdf">A Qualitative and Quantitative
		//  Evaluation of 8 Clear Sky Models</a>).
		APPROXIMATE,
		// Render the sRGB luminance, precomputed from 15 spectral radiance values
		// (see section 4.4 in <a href=
		// "http://www.oskee.wz.cz/stranka/uploads/SCCG10ElekKmoch.pdf">Real-time
		//  Spectral Scattering in Large-scale Natural Participating Media</a>).
		PRECOMPUTED
	};

	void Initialize(ColorBuffer* sceneBuffer, ColorBuffer* depthBuffer = nullptr);
	void InitModel();
	void Update();
	void UpdateUI(bool showUI);
	void Precompute(uint32_t numScatteringOrders);
}