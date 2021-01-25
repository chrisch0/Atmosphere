#include "stdafx.h"
#include "AtmosphereConstants.h"
#include "Atmosphere.h"

namespace Atmosphere
{
	bool UseConstantSolarSpectrum = false;
	bool UseOzone = true;
	bool UseCombinedTextures = false;
	bool UseHalfPrecision = false;
	Luminance UseLuminance = PRECOMPUTED;
	bool DoWhiteBalance = false;
	float SunZenithAngleRadians = 1.3f;
	float SunAzimuthAngleRadians = 2.9f;
	float Exposure = 10.0f;

	ColorBuffer* SceneColorBuffer;

	struct  
	{
		AtmosphereParameters atmosphere;
		XMFLOAT3 skySpectralRadianceToLuminance;
		XMFLOAT3 sunSpectralRadianceToLuminance;
	}PassConstants;

	float InterpolateByLambda(
		const std::vector<double>& wavelengths,
		const std::vector<double>& wavelengthFunction,
		double wavelength
	);

	Vector3 InterpolateByRGBLambda(
		const std::vector<double>& wavelengths, 
		const std::vector<double>& wavelengthFunction,
		const Vector3& lambda, double scale = 1.0
	);

	Vector3 CIE2DegColorMatchingFunction(double lambda);
	Vector3 CIEXYZTosRGB(const Vector3& XYZ);
	Vector3 LambdaTosRGB(double lambda);

	Vector3 ComputeSpectralRadianceToLuminanceFactors(
		const std::vector<double>& wavelengths,
		const std::vector<double>& solarIrradiance,
		float lambdaPower
	);

	void Initialize(ColorBuffer* sceneBuffer, ColorBuffer* depthBuffer /* = nullptr */)
	{
		SceneColorBuffer = sceneBuffer;
		InitModel();
	}

	void InitModel()
	{
		// 太阳光谱辐照度
		// Values from "Reference Solar Spectral Irradiance: ASTM G-173", ETR column
		// (see http://rredc.nrel.gov/solar/spectra/am1.5/ASTMG173/ASTMG173.html),
		// summed and averaged in each bin (e.g. the value for 360nm is the average
		// of the ASTM G-173 values for all wavelengths between 360 and 370nm).
		// Values in W.m^-2.
		constexpr int kLambdaMin = 360;
		constexpr int kLambdaMax = 830;
		constexpr double kSolarIrradiance[48] = {
			1.11776, 1.14259, 1.01249, 1.14716, 1.72765, 1.73054, 1.6887, 1.61253,
			1.91198, 2.03474, 2.02042, 2.02212, 1.93377, 1.95809, 1.91686, 1.8298,
			1.8685, 1.8931, 1.85149, 1.8504, 1.8341, 1.8345, 1.8147, 1.78158, 1.7533,
			1.6965, 1.68194, 1.64654, 1.6048, 1.52143, 1.55622, 1.5113, 1.474, 1.4482,
			1.41018, 1.36775, 1.34188, 1.31429, 1.28303, 1.26758, 1.2367, 1.2082,
			1.18737, 1.14683, 1.12362, 1.1058, 1.07124, 1.04992
		};
		// 233K的臭氧分子的横截面积
		// Values from http://www.iup.uni-bremen.de/gruppen/molspec/databases/referencespectra/o3spectra2011/index.html
		// for 233K, summed and averaged in
		// each bin (e.g. the value for 360nm is the average of the original values
		// for all wavelengths between 360 and 370nm). Values in m^2.
		constexpr double kOzoneCrossSection[48] = {
			1.18e-27, 2.182e-28, 2.818e-28, 6.636e-28, 1.527e-27, 2.763e-27, 5.52e-27,
			8.451e-27, 1.582e-26, 2.316e-26, 3.669e-26, 4.924e-26, 7.752e-26, 9.016e-26,
			1.48e-25, 1.602e-25, 2.139e-25, 2.755e-25, 3.091e-25, 3.5e-25, 4.266e-25,
			4.672e-25, 4.398e-25, 4.701e-25, 5.019e-25, 4.305e-25, 3.74e-25, 3.215e-25,
			2.662e-25, 2.238e-25, 1.852e-25, 1.473e-25, 1.209e-25, 9.423e-26, 7.455e-26,
			6.566e-26, 5.105e-26, 4.15e-26, 4.228e-26, 3.237e-26, 2.451e-26, 2.801e-26,
			2.534e-26, 1.624e-26, 1.465e-26, 2.078e-26, 1.383e-26, 7.105e-27
		};
		// From https://en.wikipedia.org/wiki/Dobson_unit, in molecules.m^-2.
		constexpr double kDobsonUnit = 2.687e20;
		// Maximum number density of ozone molecules, in m^-3 (computed so at to get
		// 300 Dobson units of ozone - for this we divide 300 DU by the integral of
		// the ozone density profile defined below, which is equal to 15km).
		constexpr double kMaxOzoneNumberDensity = 300.0 * kDobsonUnit / 15000.0;
		// Wavelength independent solar irradiance "spectrum" (not physically
		// realistic, but was used in the original implementation).
		constexpr double kConstantSolarIrradiance = 1.5;
		constexpr double kBottomRadius = 6360000.0;
		constexpr double kTopRadius = 6420000.0;
		constexpr double kRayleigh = 1.24062e-6;
		constexpr double kRayleighScaleHeight = 8000.0;
		constexpr double kMieScaleHeight = 1200.0;
		constexpr double kMieAngstromAlpha = 0.0;
		constexpr double kMieAngstromBeta = 5.328e-3;
		constexpr double kMieSingleScatteringAlbedo = 0.9;
		constexpr double kMiePhaseFunctionG = 0.8;
		constexpr double kGroundAlbedo = 0.1;
		const double max_sun_zenith_angle =
			(UseHalfPrecision ? 102.0 : 120.0) / 180.0 * kPi;

		DensityProfileLayer
			rayleigh_layer(0.0, 1.0, -1.0 / kRayleighScaleHeight, 0.0, 0.0);
		DensityProfileLayer mie_layer(0.0, 1.0, -1.0 / kMieScaleHeight, 0.0, 0.0);
		// Density profile increasing linearly from 0 to 1 between 10 and 25km, and
		// decreasing linearly from 1 to 0 between 25 and 40km. This is an approximate
		// profile from http://www.kln.ac.lk/science/Chemistry/Teaching_Resources/
		// Documents/Introduction%20to%20atmospheric%20chemistry.pdf (page 10).
		std::vector<DensityProfileLayer> ozone_density;
		ozone_density.push_back(
			DensityProfileLayer(25000.0, 0.0, 0.0, 1.0 / 15000.0, -2.0 / 3.0));
		ozone_density.push_back(
			DensityProfileLayer(0.0, 0.0, 0.0, -1.0 / 15000.0, 8.0 / 3.0));

		std::vector<double> wavelengths;
		std::vector<double> solar_irradiance;
		std::vector<double> rayleigh_scattering;
		std::vector<double> mie_scattering;
		std::vector<double> mie_extinction;
		std::vector<double> absorption_extinction;
		std::vector<double> ground_albedo;
		for (int l = kLambdaMin; l <= kLambdaMax; l += 10) {
			double lambda = static_cast<double>(l) * 1e-3;  // micro-meters
			double mie =
				kMieAngstromBeta / kMieScaleHeight * pow(lambda, -kMieAngstromAlpha);
			wavelengths.push_back(l);
			if (UseConstantSolarSpectrum) {
				solar_irradiance.push_back(kConstantSolarIrradiance);
			}
			else {
				solar_irradiance.push_back(kSolarIrradiance[(l - kLambdaMin) / 10]);
			}
			rayleigh_scattering.push_back(kRayleigh * pow(lambda, -4));
			mie_scattering.push_back(mie * kMieSingleScatteringAlbedo);
			mie_extinction.push_back(mie);
			absorption_extinction.push_back(UseOzone ?
				kMaxOzoneNumberDensity * kOzoneCrossSection[(l - kLambdaMin) / 10] :
				0.0);
			ground_albedo.push_back(kGroundAlbedo);
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
			sky_radiance_to_luminance = ComputeSpectralRadianceToLuminanceFactors(wavelengths, solar_irradiance, -4);
		}
		// Compute the values for the SUN_RADIANCE_TO_LUMINANCE constant.
		Vector3 sun_radiance_to_luminance = ComputeSpectralRadianceToLuminanceFactors(wavelengths, solar_irradiance, 0);
		PassConstants.atmosphere.
	}

	float InterpolateByLambda(const std::vector<double>& wavelengths, const std::vector<double>& wavelengthFunction, double wavelength)
	{
		assert(wavelengthFunction.size() == wavelengths.size());
		if (wavelength < wavelengths[0])
			return (float)wavelengthFunction[0];
		for (uint32_t i = 0; i < wavelengths.size(); ++i)
		{
			if (wavelength < wavelengths[i + 1])
			{
				float u = (float)((wavelength - wavelengths[i]) / (wavelengths[i + 1] - wavelengths[i]));
				return (float)(wavelengthFunction[i] * (1.0f - u) + wavelengthFunction[i + 1] * u);
			}
		}
		return (float)(wavelengthFunction.back());
	}

	Vector3 InterpolateByRGBLambda(const std::vector<double>& wavelengths, const std::vector<double>& wavelengthFunction, const Vector3& lambda, double scale)
	{
		float r = InterpolateByLambda(wavelengths, wavelengthFunction, lambda.GetX()) * scale;
		float g = InterpolateByLambda(wavelengths, wavelengthFunction, lambda.GetY()) * scale;
		float b = InterpolateByLambda(wavelengths, wavelengthFunction, lambda.GetZ()) * scale;
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
	Vector3 ComputeSpectralRadianceToLuminanceFactors(const std::vector<double>& wavelengths, const std::vector<double>& solarIrradiance, float lambdaPower)
	{
		Vector3 radiance_to_luminance(0.0);
		Vector3 solar_rgb = InterpolateByRGBLambda(wavelengths, solarIrradiance, Vector3(kLambdaR, kLambdaG, kLambdaB));
		int dlambda = 1;
		for (int lambda = kLambdaMin; lambda < kLambdaMax; lambda += dlambda)
		{
			Vector3 rgb_lambda = LambdaTosRGB(lambda);
			float solar_irradiance_lambda = InterpolateByLambda(wavelengths, solarIrradiance, lambda);
			radiance_to_luminance += rgb_lambda * Pow(lambda / rgb_lambda, Vector3(lambdaPower)) * solar_irradiance_lambda / solar_rgb * dlambda;
		}
		radiance_to_luminance *= Vector3((float)MAX_LUMINOUS_EFFICACY);
		return radiance_to_luminance;
	}
}