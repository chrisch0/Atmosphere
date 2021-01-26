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

	struct DensityProfileLayer
	{
		DensityProfileLayer() : DensityProfileLayer(0.0, 0.0, 0.0, 0.0, 0.0) {}
		DensityProfileLayer(float width, float exp_term, float exp_scale, float linear_term, float constant_term)
			: width(width), expTerm(exp_term), expScale(exp_scale), linearTerm(linear_term), constantTerm(constant_term)
		{}
		float width;
		float expTerm;
		float expScale;
		float linearTerm;
		float constantTerm;
	};

	struct AtmosphereParameters {
		XMFLOAT3 solar_irradiance;
		float sun_angular_radius;
		float bottom_radius;
		float top_radius;
		DensityProfileLayer rayleigh_density[2];
		XMFLOAT3 rayleigh_scattering;
		DensityProfileLayer mie_density[2];
		XMFLOAT3 mie_scattering;
		XMFLOAT3 mie_extinction;
		float mie_phase_function_g;
		DensityProfileLayer absorption_density[2];
		XMFLOAT3 absorption_extinction;
		XMFLOAT3 ground_albedo;
		float mu_s_min;
	};

	void Initialize(ColorBuffer* sceneBuffer, ColorBuffer* depthBuffer = nullptr);
	void InitModel();
	void Update();
	void UpdateUI();
	void Precompute(uint32_t numScatteringOrders);
}