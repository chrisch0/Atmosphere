#include "stdafx.h"
#include "AtmosphereConstants.h"
#include "Atmosphere.h"
#include "D3D12/ColorBuffer.h"
#include "D3D12/RootSignature.h"
#include "D3D12/PipelineState.h"
#include "D3D12/CommandContext.h"

#include "CompiledShaders/ComputeTransmittance_CS.h"
#include "CompiledShaders/ComputeSingleScattering_CS.h"
#include "COmpiledShaders/ComputeCombinedSingleScattering_CS.h"
#include "CompiledShaders/ComputeMultipleScattering_CS.h"
#include "CompiledShaders/ComputeDirectIrradiance_CS.h"
#include "CompiledShaders/ComputeIndirectIrradiance_CS.h"
#include "CompiledShaders/ComputeScatteringDensity_CS.h"

namespace Atmosphere
{
	bool UseConstantSolarSpectrum = false;
	bool UseOzone = true;
	bool UseCombinedTextures = false;
	bool UseHalfPrecision = false;
	Luminance UseLuminance = NONE;
	bool DoWhiteBalance = false;
	float SunZenithAngleRadians = 1.3f;
	float SunAzimuthAngleRadians = 2.9f;
	float Exposure = 10.0f;

	double GroundAlbedo = 0.1;


	uint32_t NumPrecomputedWavelengths = 3;

	ColorBuffer* SceneColorBuffer;

	std::shared_ptr<ColorBuffer> Transmittance;
	std::shared_ptr<VolumeColorBuffer> Scattering;
	std::shared_ptr<VolumeColorBuffer> OptionalSingleMieScattering;
	std::shared_ptr<ColorBuffer> Irradiance;

	std::shared_ptr<ColorBuffer> InterIrradiance;
	std::shared_ptr<VolumeColorBuffer> InterRayleighScattering;
	std::shared_ptr<VolumeColorBuffer> InterMieScattering;
	std::shared_ptr<VolumeColorBuffer> InterScatteringDensity;

	RootSignature PrecomputeRS;
	ComputePSO TransmittancePSO;
	ComputePSO ScatteringDensityPSO;
	ComputePSO CombinedSingleScatteringPSO;
	ComputePSO SingleScatteringPSO;
	ComputePSO MultipleScatteringPSO;
	ComputePSO DirectIrradiancePSO;
	ComputePSO IndirectIrradiancePSO;
	ComputePSO ComputeSkyPSO;

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

	struct  
	{
		AtmosphereParameters atmosphere;
		XMFLOAT3 skySpectralRadianceToLuminance;
		float pad0;
		XMFLOAT3 sunSpectralRadianceToLuminance;
		float pad1;
	}PassConstants;

	XMFLOAT4X4 LuminanceFromRadiance;

	std::vector<double> Wavelengths;
	std::vector<double> SolarIrradiance;
	std::vector<double> RayleighScattering;
	std::vector<double> MieScattering;
	std::vector<double> MieExtinction;
	std::vector<double> AbsorptionExtinction;
	std::vector<double> GroundAlbedos;

	float InterpolateByLambda(
		const std::vector<double>& wavelengthFunction,
		double wavelength
	);

	Vector3 InterpolateByRGBLambda(
		const std::vector<double>& wavelengthFunction,
		const Vector3& lambda, double scale = 1.0
	);

	Vector3 CIE2DegColorMatchingFunction(double lambda);
	Vector3 CIEXYZTosRGB(const Vector3& XYZ);
	Vector3 LambdaTosRGB(double lambda);

	Vector3 ComputeSpectralRadianceToLuminanceFactors(
		const std::vector<double>& solarIrradiance,
		float lambdaPower
	);

	template <typename T>
	void ReleaseOrNewTexture(std::shared_ptr<T>& texPtr)
	{
		if (texPtr == nullptr)
			texPtr = std::make_shared<T>();
		else
			texPtr->Destroy();
	}

	void InitTextures();
	void InitIntermediateTextures();
	void InitPSO();

	void Precompute(ComputeContext& context, const Vector3& lambdas, uint32_t numScatteringOrders);

	void UpdateLambdaDependsCB(const Vector3& lambdas);
	void UpdatePhysicalCB(const Vector3& lambdas);
	void UpdateModel();

	void Initialize(ColorBuffer* sceneBuffer, ColorBuffer* depthBuffer /* = nullptr */)
	{
		SceneColorBuffer = sceneBuffer;
		InitPSO();
		InitModel();
		InitTextures();
		// TODO: do in precompute and release after precompute complete.
		InitIntermediateTextures();
	}

	void InitPSO()
	{
		PrecomputeRS.Reset(5, 1);
		PrecomputeRS[0].InitAsConstantBufferView(0);
		PrecomputeRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 4);
		PrecomputeRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 5);
		PrecomputeRS[3].InitAsConstantBufferView(1);
		PrecomputeRS[4].InitAsConstantBufferView(2);
		PrecomputeRS.InitStaticSampler(0, Global::SamplerLinearClampDesc);
		PrecomputeRS.Finalize(L"PrecomputeRS");

		TransmittancePSO.SetRootSignature(PrecomputeRS);
		TransmittancePSO.SetComputeShader(g_pComputeTransmittance_CS, sizeof(g_pComputeTransmittance_CS));
		TransmittancePSO.Finalize();

		SingleScatteringPSO.SetRootSignature(PrecomputeRS);
		SingleScatteringPSO.SetComputeShader(g_pComputeSingleScattering_CS, sizeof(g_pComputeSingleScattering_CS));
		SingleScatteringPSO.Finalize();

		CombinedSingleScatteringPSO.SetRootSignature(PrecomputeRS);
		CombinedSingleScatteringPSO.SetComputeShader(g_pComputeCombinedSingleScattering_CS, sizeof(g_pComputeCombinedSingleScattering_CS));
		CombinedSingleScatteringPSO.Finalize();

		ScatteringDensityPSO.SetRootSignature(PrecomputeRS);
		ScatteringDensityPSO.SetComputeShader(g_pComputeScatteringDensity_CS, sizeof(g_pComputeScatteringDensity_CS));
		ScatteringDensityPSO.Finalize();

		MultipleScatteringPSO.SetRootSignature(PrecomputeRS);
		MultipleScatteringPSO.SetComputeShader(g_pComputeMultipleScattering_CS, sizeof(g_pComputeMultipleScattering_CS));
		MultipleScatteringPSO.Finalize();

		DirectIrradiancePSO.SetRootSignature(PrecomputeRS);
		DirectIrradiancePSO.SetComputeShader(g_pComputeDirectIrradiance_CS, sizeof(g_pComputeDirectIrradiance_CS));
		DirectIrradiancePSO.Finalize();

		IndirectIrradiancePSO.SetRootSignature(PrecomputeRS);
		IndirectIrradiancePSO.SetComputeShader(g_pComputeIndirectIrradiance_CS, sizeof(g_pComputeIndirectIrradiance_CS));
		IndirectIrradiancePSO.Finalize();
	}

	void InitModel()
	{
		const double max_sun_zenith_angle =
			(UseHalfPrecision ? 102.0 : 120.0) / 180.0 * kPi;

		DensityProfileLayer
			rayleigh_layer(0.0f / (float)kLengthUnitInMeters, 1.0f, (float)(-1.0 / kRayleighScaleHeight * kLengthUnitInMeters), 0.0f * (float)kLengthUnitInMeters, 0.0f);
		DensityProfileLayer mie_layer(0.0f / (float)kLengthUnitInMeters, 1.0f, (float)(-1.0 / kMieScaleHeight * kLengthUnitInMeters), 0.0f * (float)kLengthUnitInMeters, 0.0f);
		// Density profile increasing linearly from 0 to 1 between 10 and 25km, and
		// decreasing linearly from 1 to 0 between 25 and 40km. This is an approximate
		// profile from http://www.kln.ac.lk/science/Chemistry/Teaching_Resources/
		// Documents/Introduction%20to%20atmospheric%20chemistry.pdf (page 10).
		std::vector<DensityProfileLayer> ozone_density;
		ozone_density.push_back(
			DensityProfileLayer(25000.0f / (float)kLengthUnitInMeters, 0.0f, 0.0f * (float)kLengthUnitInMeters, 1.0f / 15000.0f * (float)kLengthUnitInMeters, -2.0f / 3.0f));
		ozone_density.push_back(
			DensityProfileLayer(0.0f / (float)kLengthUnitInMeters, 0.0f, 0.0f * (float)kLengthUnitInMeters, -1.0f / 15000.0f * (float)kLengthUnitInMeters, 8.0f / 3.0f));

		for (int l = kLambdaMin; l <= kLambdaMax; l += 10) 
		{
			double lambda = static_cast<double>(l) * 1e-3;  // micro-meters
			double mie =
				kMieAngstromBeta / kMieScaleHeight * pow(lambda, -kMieAngstromAlpha);
			Wavelengths.push_back(l);
			if (UseConstantSolarSpectrum) 
			{
				SolarIrradiance.push_back(kConstantSolarIrradiance);
			}
			else 
			{
				SolarIrradiance.push_back(kSolarIrradiance[(l - kLambdaMin) / 10]);
			}
			RayleighScattering.push_back(kRayleigh * pow(lambda, -4));
			MieScattering.push_back(mie * kMieSingleScatteringAlbedo);
			MieExtinction.push_back(mie);
			AbsorptionExtinction.push_back(UseOzone ?
				kMaxOzoneNumberDensity * kOzoneCrossSection[(l - kLambdaMin) / 10] :
				0.0);
			GroundAlbedos.push_back(GroundAlbedo);
		}

		int num_precomputed_wavelengths = UseLuminance == PRECOMPUTED ? 15 : 3;
		bool precompute_illuminance = num_precomputed_wavelengths > 3;
		// Compute the values for the SKY_RADIANCE_TO_LUMINANCE constant. In theory
		// this should be 1 in precomputed illuminance mode (because the precomputed
		// textures already contain illuminance values). In practice, however, storing
		// true illuminance values in half precision textures yields artefacts
		// (because the values are too large), so we store illuminance values divided
		// by MAX_LUMINOUS_EFFICACY instead. This is why, in precomputed illuminance
		// mode, we set SKY_RADIANCE_TO_LUMINANCE to MAX_LUMINOUS_EFFICACY.
		Vector3 sky_radiance_to_luminance;
		if (precompute_illuminance)
		{
			sky_radiance_to_luminance = Vector3((float)MAX_LUMINOUS_EFFICACY);
		}
		else
		{
			// Compute the values for the SUN_RADIANCE_TO_LUMINANCE constant.
			sky_radiance_to_luminance = ComputeSpectralRadianceToLuminanceFactors(SolarIrradiance, -4);
		}
		Vector3 sun_radiance_to_luminance = ComputeSpectralRadianceToLuminanceFactors(SolarIrradiance, 0);
		Vector3 lambda_rgb(kLambdaR, kLambdaG, kLambdaB);

		// Update constant buffer
		UpdateLambdaDependsCB(lambda_rgb);
		PassConstants.atmosphere.sun_angular_radius = (float)kSunAngularRadius;
		PassConstants.atmosphere.bottom_radius = (float)(kBottomRadius / kLengthUnitInMeters);
		PassConstants.atmosphere.top_radius = (float)(kTopRadius / kLengthUnitInMeters);
		PassConstants.atmosphere.rayleigh_density[0] = DensityProfileLayer();
		PassConstants.atmosphere.rayleigh_density[1] = rayleigh_layer;
		PassConstants.atmosphere.mie_density[0] = DensityProfileLayer();
		PassConstants.atmosphere.mie_density[1] = mie_layer;
		PassConstants.atmosphere.mie_phase_function_g = (float)kMiePhaseFunctionG;
		PassConstants.atmosphere.absorption_density[0] = ozone_density[0];
		PassConstants.atmosphere.absorption_density[1] = ozone_density[1];
		PassConstants.atmosphere.mu_s_min = std::cosf((float)max_sun_zenith_angle);
		XMStoreFloat3(&PassConstants.skySpectralRadianceToLuminance, sky_radiance_to_luminance);
		XMStoreFloat3(&PassConstants.sunSpectralRadianceToLuminance, sun_radiance_to_luminance);
	}

	void UpdateModel()
	{
		constexpr double kConstantSolarIrradiance = 1.5;
		const double max_sun_zenith_angle =
			(UseHalfPrecision ? 102.0 : 120.0) / 180.0 * kPi;

		SolarIrradiance.clear();
		AbsorptionExtinction.clear();
		GroundAlbedos.clear();

		for (int l = kLambdaMin; l <= kLambdaMax; l += 10) 
		{
			double lambda = static_cast<double>(l) * 1e-3;  // micro-meters
			if (UseConstantSolarSpectrum) 
			{
				SolarIrradiance.push_back(kConstantSolarIrradiance);
			}
			else 
			{
				SolarIrradiance.push_back(kSolarIrradiance[(l - kLambdaMin) / 10]);
			}
			AbsorptionExtinction.push_back(UseOzone ?
				kMaxOzoneNumberDensity * kOzoneCrossSection[(l - kLambdaMin) / 10] :
				0.0);
			GroundAlbedos.push_back(GroundAlbedo);
		}

		int num_precomputed_wavelengths = UseLuminance == PRECOMPUTED ? 15 : 3;
		bool precompute_illuminance = num_precomputed_wavelengths > 3;
		Vector3 sky_radiance_to_luminance;
		if (precompute_illuminance)
		{
			sky_radiance_to_luminance = Vector3((float)MAX_LUMINOUS_EFFICACY);
		}
		else
		{
			// Compute the values for the SUN_RADIANCE_TO_LUMINANCE constant.
			sky_radiance_to_luminance = ComputeSpectralRadianceToLuminanceFactors(SolarIrradiance, -4);
		}
		Vector3 sun_radiance_to_luminance = ComputeSpectralRadianceToLuminanceFactors(SolarIrradiance, 0);

		Vector3 lambda_rgb(kLambdaR, kLambdaG, kLambdaB);
		XMStoreFloat3(&PassConstants.atmosphere.solar_irradiance, InterpolateByRGBLambda(SolarIrradiance, lambda_rgb, 1.0));
		XMStoreFloat3(&PassConstants.atmosphere.absorption_extinction, InterpolateByRGBLambda(AbsorptionExtinction, lambda_rgb, kLengthUnitInMeters));
		XMStoreFloat3(&PassConstants.atmosphere.ground_albedo, InterpolateByRGBLambda(GroundAlbedos, lambda_rgb, 1.0));
		XMStoreFloat3(&PassConstants.skySpectralRadianceToLuminance, sky_radiance_to_luminance);
		XMStoreFloat3(&PassConstants.sunSpectralRadianceToLuminance, sun_radiance_to_luminance);
	}

	void InitTextures()
	{
		ReleaseOrNewTexture(Transmittance);
		Transmittance->Create(L"Transmittance", TRANSMITTANCE_TEXTURE_WIDTH, TRANSMITTANCE_TEXTURE_HEIGHT, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);

		ReleaseOrNewTexture(Scattering);
		Scattering->Create(L"Scattering", SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT, SCATTERING_TEXTURE_DEPTH, 1, UseHalfPrecision ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R32G32B32A32_FLOAT);
		
		if (!UseCombinedTextures)
		{
			ReleaseOrNewTexture(OptionalSingleMieScattering);
			OptionalSingleMieScattering->Create(L"Optional Single Mie", SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT, SCATTERING_TEXTURE_DEPTH, 1, UseHalfPrecision ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R32G32B32A32_FLOAT);
		}

		ReleaseOrNewTexture(Irradiance);
		Irradiance->Create(L"Irradiance", IRRADIANCE_TEXTURE_WIDTH, IRRADIANCE_TEXTURE_HEIGHT, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
	}

	void InitIntermediateTextures()
	{
		ReleaseOrNewTexture(InterIrradiance);
		InterIrradiance->Create(L"Intermediate Irradiance", IRRADIANCE_TEXTURE_WIDTH, IRRADIANCE_TEXTURE_HEIGHT, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);

		ReleaseOrNewTexture(InterRayleighScattering);
		InterRayleighScattering->Create(L"Intermediate Rayleigh Scattering", SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT, SCATTERING_TEXTURE_DEPTH, 1, UseHalfPrecision ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R32G32B32A32_FLOAT);

		ReleaseOrNewTexture(InterMieScattering);
		InterMieScattering->Create(L"Intermediate Mie Scattering", SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT, SCATTERING_TEXTURE_DEPTH, 1, UseHalfPrecision ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R32G32B32A32_FLOAT);

		ReleaseOrNewTexture(InterScatteringDensity);
		InterScatteringDensity->Create(L"Intermediate Scattering Density", SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT, SCATTERING_TEXTURE_DEPTH, 1, UseHalfPrecision ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R32G32B32A32_FLOAT);
	}

	void Precompute(uint32_t numScatteringOrders)
	{
		ComputeContext& context = ComputeContext::Begin();
		if (NumPrecomputedWavelengths <= 3)
		{
			Vector3 lambda_rgb(kLambdaR, kLambdaG, kLambdaB);
			XMStoreFloat4x4(&LuminanceFromRadiance, Matrix4(kIdentity));
			Precompute(context, lambda_rgb, numScatteringOrders);
		}
		else
		{
			int num_iterators = (NumPrecomputedWavelengths) / 3;
			double dlambda = (kLambdaMax - kLambdaMin) / (3 * num_iterators);
			for (int i = 0; i < num_iterators; ++i)
			{
				
				double lambda_r = kLambdaMin + (3.0 * i + 0.5) * dlambda;
				double lambda_g = kLambdaMin + (3.0 * i + 1.5) * dlambda;
				double lambda_b = kLambdaMin + (3.0 * i + 2.5) * dlambda;
				/*Matrix4 luminance_from_radiance(
					LambdaTosRGB(lambda_r),
					LambdaTosRGB(lambda_g),
					LambdaTosRGB(lambda_b)
				);
				XMStoreFloat3x3(&LuminanceFromRadiance, luminance_from_radiance);
				Precompute(context, Vector3((float)lambda_r, (float)lambda_g, (float)lambda_b), numScatteringOrders);*/
			}
		}
		context.Finish();
	}

	void Precompute(ComputeContext& context, const Vector3& lambdas, uint32_t numScatteringOrders)
	{
		// Precompute transmittance
		context.TransitionResource(*Transmittance, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		context.SetRootSignature(PrecomputeRS);
		context.SetPipelineState(TransmittancePSO);
		context.SetDynamicDescriptor(1, 0, Transmittance->GetUAV());
		context.SetDynamicConstantBufferView(3, sizeof(PassConstants), &PassConstants);
		context.Dispatch2D(Transmittance->GetWidth(), Transmittance->GetHeight());

		// Precompute direct ground irradiance
		context.TransitionResource(*InterIrradiance, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		context.TransitionResource(*Irradiance, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		context.SetPipelineState(DirectIrradiancePSO);
		context.SetDynamicDescriptor(1, 0, InterIrradiance->GetUAV());
		context.SetDynamicDescriptor(1, 1, Irradiance->GetUAV());
		context.SetDynamicDescriptor(2, 0, Transmittance->GetSRV());
		context.Dispatch2D(IRRADIANCE_TEXTURE_WIDTH, IRRADIANCE_TEXTURE_HEIGHT);
		
		// Precompute single rayleigh and single mie
		context.TransitionResource(*Transmittance, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		context.TransitionResource(*InterRayleighScattering, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		context.TransitionResource(*InterMieScattering, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		context.TransitionResource(*Scattering, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		if (!UseCombinedTextures)
			context.TransitionResource(*OptionalSingleMieScattering, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		context.SetPipelineState(SingleScatteringPSO);
		context.SetDynamicConstantBufferView(4, sizeof(LuminanceFromRadiance), &LuminanceFromRadiance);
		context.SetDynamicDescriptor(1, 0, InterRayleighScattering->GetUAV());
		context.SetDynamicDescriptor(1, 1, InterMieScattering->GetUAV());
		context.SetDynamicDescriptor(1, 2, Scattering->GetUAV());
		if (!UseCombinedTextures)
			context.SetDynamicDescriptor(1, 3, OptionalSingleMieScattering->GetUAV());
		context.SetDynamicDescriptor(2, 0, Transmittance->GetSRV());
		context.Dispatch3D(SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT, SCATTERING_TEXTURE_DEPTH, 8, 8, 8);

		// Compute 2nd, 3rd, 4th... scattering
		context.TransitionResource(*InterMieScattering, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		//context.TransitionResource(*Irradiance, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		for (uint32_t scattering_order = 2; scattering_order <= numScatteringOrders; ++scattering_order)
		{
			// Compute scattering density, store the value in InterScatteringDensity texture
			context.TransitionResource(*InterScatteringDensity, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			context.TransitionResource(*InterRayleighScattering, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			context.TransitionResource(*InterIrradiance, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			context.SetPipelineState(ScatteringDensityPSO);
			context.SetDynamicConstantBufferView(0, sizeof(scattering_order), &scattering_order);
			context.SetDynamicDescriptor(1, 0, InterScatteringDensity->GetUAV());
			context.SetDynamicDescriptor(2, 0, Transmittance->GetSRV());
			context.SetDynamicDescriptor(2, 1, InterRayleighScattering->GetSRV());
			context.SetDynamicDescriptor(2, 2, InterMieScattering->GetSRV());
			context.SetDynamicDescriptor(2, 3, InterRayleighScattering->GetSRV());
			context.SetDynamicDescriptor(2, 4, InterIrradiance->GetSRV());
			context.Dispatch3D(SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT, SCATTERING_TEXTURE_DEPTH);

			// Compute indirect ground irradiance
			context.TransitionResource(*InterIrradiance, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			context.SetPipelineState(IndirectIrradiancePSO);
			context.SetDynamicDescriptor(1, 0, InterIrradiance->GetUAV());
			context.SetDynamicDescriptor(1, 1, Irradiance->GetUAV());
			context.SetDynamicDescriptor(2, 0, InterRayleighScattering->GetSRV());
			context.SetDynamicDescriptor(2, 1, InterMieScattering->GetSRV());
			context.SetDynamicDescriptor(2, 2, InterRayleighScattering->GetSRV());
			context.Dispatch2D(IRRADIANCE_TEXTURE_WIDTH, IRRADIANCE_TEXTURE_HEIGHT);

			// Compute multiple scattering, store in inter
			context.TransitionResource(*InterRayleighScattering, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			context.TransitionResource(*Scattering, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			context.TransitionResource(*InterScatteringDensity, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			context.SetPipelineState(MultipleScatteringPSO);
			context.SetDynamicDescriptor(1, 0, InterRayleighScattering->GetUAV());
			context.SetDynamicDescriptor(1, 1, Scattering->GetUAV());
			context.SetDynamicDescriptor(2, 0, Transmittance->GetSRV());
			context.SetDynamicDescriptor(2, 1, InterScatteringDensity->GetSRV());
			context.Dispatch3D(SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT, SCATTERING_TEXTURE_DEPTH);
		}	

		context.TransitionResource(*Transmittance, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		context.TransitionResource(*Scattering, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		context.TransitionResource(*OptionalSingleMieScattering, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		context.TransitionResource(*Irradiance, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	}

	float InterpolateByLambda(const std::vector<double>& wavelengthFunction, double wavelength)
	{
		assert(wavelengthFunction.size() == Wavelengths.size());
		if (wavelength < Wavelengths[0])
			return (float)wavelengthFunction[0];
		for (uint32_t i = 0; i < Wavelengths.size(); ++i)
		{
			if (wavelength < Wavelengths[i + 1])
			{
				float u = (float)((wavelength - Wavelengths[i]) / (Wavelengths[i + 1] - Wavelengths[i]));
				return (float)(wavelengthFunction[i] * (1.0f - u) + wavelengthFunction[i + 1] * u);
			}
		}
		return (float)(wavelengthFunction.back());
	}

	Vector3 InterpolateByRGBLambda(const std::vector<double>& wavelengthFunction, const Vector3& lambda, double scale)
	{
		float r = (float)(InterpolateByLambda(wavelengthFunction, lambda.GetX()) * scale);
		float g = (float)(InterpolateByLambda(wavelengthFunction, lambda.GetY()) * scale);
		float b = (float)(InterpolateByLambda(wavelengthFunction, lambda.GetZ()) * scale);
		return Vector3(r, g, b);
	}

	Vector3 CIE2DegColorMatchingFunction(double lambda)
	{
		if (lambda < kLambdaMin || lambda >= kLambdaMax)
			return Vector3(0.0f);
		double u = (lambda - kLambdaMin) / 5.0;
		int index = (int)std::floor(u);
		assert(index >= 0 && index < 94);
		assert(CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * index] <= lambda &&
			   CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * (index + 1)] >= lambda);
		u -= index;
		double X = CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * index + 1] * (1.0 - u) +
			CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * (index + 1) + 1] * u;
		double Y = CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * index + 2] * (1.0 - u) +
			CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * (index + 1) + 2] * u;
		double Z = CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * index + 3] * (1.0 - u) +
			CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * (index + 1) + 3] * u;
		return Vector3((float)X, (float)Y, (float)Z);
	}

	Vector3 CIEXYZTosRGB(const Vector3& XYZ)
	{
		float r = XYZ_TO_SRGB[0] * XYZ.GetX() + XYZ_TO_SRGB[1] * XYZ.GetY() + XYZ_TO_SRGB[2] * XYZ.GetZ();
		float g = XYZ_TO_SRGB[3] * XYZ.GetX() + XYZ_TO_SRGB[4] * XYZ.GetY() + XYZ_TO_SRGB[5] * XYZ.GetZ();
		float b = XYZ_TO_SRGB[6] * XYZ.GetX() + XYZ_TO_SRGB[7] * XYZ.GetY() + XYZ_TO_SRGB[8] * XYZ.GetZ();
		return Vector3(r, g, b);
	}

	Vector3 LambdaTosRGB(double lambda)
	{
		auto XYZ = CIE2DegColorMatchingFunction(lambda);
		return CIEXYZTosRGB(XYZ);
	}

	/*We can then implement a utility function to compute the "spectral radiance to
	luminance" conversion constants (see Section 14.3 in <a
	href="https://arxiv.org/pdf/1612.04336.pdf">A Qualitative and Quantitative
	Evaluation of 8 Clear Sky Models</a> for their definitions):*/
	// The returned constants are in lumen.nm / watt.
	Vector3 ComputeSpectralRadianceToLuminanceFactors(const std::vector<double>& solarIrradiance, float lambdaPower)
	{
		Vector3 radiance_to_luminance(0.0);
		static const Vector3 rbg_lambda(kLambdaR, kLambdaG, kLambdaB);
		Vector3 solar_rgb = InterpolateByRGBLambda(solarIrradiance, rbg_lambda);
		int dlambda = 1;
		for (int lambda = kLambdaMin; lambda < kLambdaMax; lambda += dlambda)
		{
			Vector3 lambda_color = LambdaTosRGB(lambda);
			float solar_irradiance_lambda = InterpolateByLambda(solarIrradiance, lambda);
			radiance_to_luminance += lambda_color * Pow((float)lambda / rbg_lambda, Vector3(lambdaPower)) * solar_irradiance_lambda / solar_rgb * (float)dlambda;
		}
		radiance_to_luminance *= Vector3((float)MAX_LUMINOUS_EFFICACY);
		return radiance_to_luminance;
	}

	void UpdateLambdaDependsCB(const Vector3& lambdas)
	{
		XMStoreFloat3(&PassConstants.atmosphere.solar_irradiance, InterpolateByRGBLambda(SolarIrradiance, lambdas, 1.0));
		XMStoreFloat3(&PassConstants.atmosphere.rayleigh_scattering, InterpolateByRGBLambda(RayleighScattering, lambdas, kLengthUnitInMeters));
		XMStoreFloat3(&PassConstants.atmosphere.mie_scattering, InterpolateByRGBLambda(MieScattering, lambdas, kLengthUnitInMeters));
		XMStoreFloat3(&PassConstants.atmosphere.mie_extinction, InterpolateByRGBLambda(MieExtinction, lambdas, kLengthUnitInMeters));
		XMStoreFloat3(&PassConstants.atmosphere.absorption_extinction, InterpolateByRGBLambda(AbsorptionExtinction, lambdas, kLengthUnitInMeters));
		XMStoreFloat3(&PassConstants.atmosphere.ground_albedo, InterpolateByRGBLambda(GroundAlbedos, lambdas, 1.0));
	}

	void UpdatePhysicalCB(const Vector3& lambdas)
	{

	}

	void UpdateUI(bool* ShowUI)
	{
		if (*ShowUI)
		{
			ImGui::Begin("Atmosphere Setting", ShowUI);
			bool dirty_flag = false;
			static bool transmittance_view_detail = false;
			static bool transmittance_detail_opening = false;
			ImGui::PreviewImageButton(Transmittance.get(), ImVec2((float)TRANSMITTANCE_TEXTURE_WIDTH, (float)TRANSMITTANCE_TEXTURE_HEIGHT), "Transmittance", &transmittance_view_detail, &transmittance_detail_opening);
			
			static bool irradiance_view_detail = false;
			static bool irradiance_detail_opening = false;
			ImGui::PreviewImageButton(Irradiance.get(), ImVec2((float)TRANSMITTANCE_TEXTURE_WIDTH, (float)TRANSMITTANCE_TEXTURE_HEIGHT), "Irradiance", &irradiance_view_detail, &irradiance_detail_opening);

			static bool scattering_view_detail = false;
			static bool scattering_detail_opening = false;
			ImGui::PreviewVolumeImageButton(Scattering.get(), ImVec2((float)SCATTERING_TEXTURE_WIDTH, (float)SCATTERING_TEXTURE_HEIGHT), "Scattering", &scattering_view_detail, &scattering_detail_opening);
			
			if (!UseCombinedTextures)
			{
				static bool optional_scattering_view = false;
				static bool optional_scattering_opening = false;
				ImGui::PreviewVolumeImageButton(OptionalSingleMieScattering.get(), ImVec2((float)SCATTERING_TEXTURE_WIDTH, (float)SCATTERING_TEXTURE_HEIGHT), "Scattering", &optional_scattering_view, &optional_scattering_opening);
			}
			
			static int i = 0;
			//if (i == 0)
			if (ImGui::Button("Precompute"))
			{
				Precompute(2);
				++i;
			}
			ImGui::End();
		}
	}
}