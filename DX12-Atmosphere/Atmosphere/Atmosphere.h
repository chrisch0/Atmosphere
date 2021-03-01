#pragma once
#include "stdafx.h"

class ColorBuffer;
class Camera;

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
		float pad[3];
	};

	struct AtmosphereParameters {
		XMFLOAT3 solar_irradiance;
		float sun_angular_radius;
		XMFLOAT3 absorption_extinction;
		float bottom_radius;
		XMFLOAT3 ground_albedo;
		float top_radius;
		XMFLOAT3 rayleigh_scattering;
		float mie_phase_function_g;
		XMFLOAT3 mie_scattering;
		float mu_s_min;
		XMFLOAT3 mie_extinction;
		float pad;
		DensityProfileLayer rayleigh_density[2];
		DensityProfileLayer mie_density[2];
		DensityProfileLayer absorption_density[2];
	};

	struct AtmosphereCB
	{
		AtmosphereParameters atmosphere;
		XMFLOAT3 skySpectralRadianceToLuminance;
		float pad0;
		XMFLOAT3 sunSpectralRadianceToLuminance;
		float pad1;
	};

	struct RenderCB
	{
		Matrix4 invView;
		Matrix4 invProj;
		XMFLOAT3 cameraPosition;
		float exposure;
		XMFLOAT3 lightDir;
		float sunSize;
		XMFLOAT4 resolution;
		XMFLOAT3 whitePoint;
		float pad;
		XMFLOAT3 earthCenter;
		float pad1;
		XMFLOAT3 groundAlbedo;
	};

	void Initialize(ColorBuffer* sceneBuffer, ColorBuffer* depthBuffer = nullptr);
	void InitModel();
	void Update(const Vector3& lightDir, const Vector4& resolution);
	void Draw();
	void UpdateUI(bool* showUI);
	void Precompute(uint32_t numScatteringOrders);
	void SetCamera(Camera* camera);

	ColorBuffer* GetTransmittance();
	ColorBuffer* GetIrradiance();
	VolumeColorBuffer* GetScattering();
	VolumeColorBuffer* GetOptionalScattering();
	AtmosphereCB* GetAtmosphereCB();

}