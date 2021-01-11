#include "Random.hlsli"

cbuffer PassConstants : register(b0)
{
	float3 ViewerPos;
	float Time;
	int SampleCountMin;
	int SampleCountMax;
	float Extinct;
	float HGCoeff;
	float Scatter;
	float3 LightDir;
	int LightSampleCount;
	float3 LightColor;
	float AltitudeMin;
	float AltitudeMax;
	float FarDistance;
};

Texture3D<float4> BasicCloudShape : register(t0);
Texture2D<float4> DensityHeightGradient : register(t1);
Texture3D<float4> ErosionTexture : register(t2);
Texture3D<float> PerlinNoise : register(t4);
Texture2D<float3> WeatherTexture : register(t5);

SamplerState LinearRepeatSampler : register(s0);

static int SampleCount = 0;

struct PSInput
{
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
	float3 rayDir : TEXCOORD1;
	float3 skyColor : TEXCOORD2;
	float3 groundColor : TEXCOORD3;
};

float UVRandom(float2 uv)
{
	float f = dot(float2(12.9898, 78.233), uv);
	return frac(43758.5453 * sin(f));
}

float HenyeyGreenstein(float3 lightDir, float3 viewDir, float G)
{
	float cos_angle = dot(lightDir, viewDir);
	return ((1.0f - G * G) / pow((1.0f + G * G - 2.0f * G * cos_angle), 1.5f)) / (4.0f * 3.1415926f);
}

float Beer(float depth)
{
	return exp(-Extinct * depth);
}

float BeerPowder(float depth)
{
	return 2.0 * exp(-Extinct * depth) * (1 - exp( -2 * depth));
}

float Remap(float originalValue, float originalMin, float originalMax,
	float newMin, float newMax)
{
	return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
}

float3 GetWeatherData(float3 posWS)
{
	//float2 uv = (posWS.xz) * 0.0005f;
	float2 uv = (posWS.xz + 10000.0f) * 0.00005f;
	SampleCount++;
	return WeatherTexture.Sample(LinearRepeatSampler, uv);
}

float GetHeightFractionForPoint(float3 posWS)
{
	float height_fraction = (posWS.y - AltitudeMin) / (AltitudeMax - AltitudeMin);
	return saturate(height_fraction);
}

float GetDensityHeightGradientForPoint(float3 posWS, float3 weatherData)
{
	float height_fraction = GetHeightFractionForPoint(posWS);
	SampleCount++;
	return DensityHeightGradient.SampleLevel(LinearRepeatSampler, float2(weatherData.g, height_fraction), 0).r;
}

float SampleNoise(float3 posWS)
{
	const float scale = 0.0001f;
	
	float3 uvw = (posWS + 5000) * scale;
	SampleCount++;
	float4 low_frequency_noises = BasicCloudShape.SampleLevel(LinearRepeatSampler, uvw, 0);
	float low_freq_FBM =
		(low_frequency_noises.g * 0.625f) +
		(low_frequency_noises.b * 0.25f) +
		(low_frequency_noises.a * 0.125f);
	float base_cloud = Remap(low_frequency_noises.r, -(1.0f - low_freq_FBM), 1.0, 0.0, 1.0);
	
	float3 weather_data = GetWeatherData(posWS);
	float density_height_gradient = GetDensityHeightGradientForPoint(posWS, weather_data);
	
	return base_cloud * pow(weather_data.r, 2.0) * density_height_gradient;

	/*base_cloud *= density_height_gradient;
	float cloud_coverage = 0.5;
	float base_cloud_with_coverage = Remap(base_cloud, cloud_coverage, 1.0, 0.0, 1.0);
	base_cloud_with_coverage *= cloud_coverage;
	return base_cloud_with_coverage;*/
}

float SampleCloudDensity(float3 posWS, float step, float3 weatherData, bool cheapSample)
{
	float density = 0.0f;
	const float scale = 0.00005f;
	float3 uvw = posWS * scale;

	SampleCount++;
	float4 low_frequency_noises = BasicCloudShape.SampleLevel(LinearRepeatSampler, uvw, 0);
	float low_freq_FBM =
		(low_frequency_noises.g * 0.625f) +
		(low_frequency_noises.b * 0.25f) +
		(low_frequency_noises.a * 0.125f);
	float base_cloud = Remap(low_frequency_noises.r, -(1.0f - low_freq_FBM), 1.0, 0.0, 1.0);

	float density_height_gradient = GetDensityHeightGradientForPoint(posWS, weatherData);

	base_cloud *= density_height_gradient;

	return density;
}

float SampleCloudDensityAlongRay(float3 posWS)
{
	float density_along_cone = 0.0;
	float stride = (AltitudeMax - posWS.y) / (LightDir.y * 6.0);
	float3 light_step = LightDir * stride;
	float cone_spread_multiplier = stride * 0.6;
	for (int i = 0; i <= 6; i++)
	{
		posWS += light_step + (cone_spread_multiplier * GetRandVec3(1234, floor(posWS)) * floor(i));
		float3 weather_data = GetWeatherData(posWS);
		if (density_along_cone < 0.3)
		{
			density_along_cone += SampleCloudDensity(posWS, stride, weather_data, false);
		}
		else
		{
			density_along_cone += SampleCloudDensity(posWS, stride, weather_data, true);
		}
	}
	return density_along_cone;
}

float Raymarch(float3 startWS, float step, int sampleCount)
{
	float density = 0.0;
	float cloud_test = 0.0;
	int zero_density_sample_count = 0;
	float3 posWS = startWS;

	for (int i = 0; i < sampleCount; ++i)
	{
		float3 weather_data = GetWeatherData(posWS);
		if (cloud_test > 0.0)
		{
			float sampled_density = SampleCloudDensity(posWS, step, weather_data, false);
			float density_along_light_ray = 0.0;

			if (sampled_density == 0.0)
			{
				zero_density_sample_count++;
			}
			
			if (zero_density_sample_count != 6)
			{
				density += sampled_density;
				if (sampled_density != 0.0)
				{
					density_along_light_ray = SampleCloudDensityAlongRay(posWS);
				}
				posWS += step;
			}
			else
			{
				cloud_test = 0.0;
				zero_density_sample_count = 0;
			}
		}
		else
		{
			cloud_test = SampleCloudDensity(posWS, step, weather_data, true);
			if (cloud_test == 0.0)
				posWS += step;
		}
	}
	return density;
}

float MarchLight(float3 posWS, float rand)
{
	float3 pos = posWS;
	float stride = (AltitudeMax - pos.y) / (LightDir.y * LightSampleCount);
	pos += LightDir * stride * rand;

	float depth = 0;
	[loop]
	for (int s = 0; s < LightSampleCount; ++s)
	{
		depth += SampleNoise(pos) * stride;
		pos += LightDir * stride;
	}
	return BeerPowder(depth);
}

float4 main(PSInput psInput) : SV_Target
{
	float3 viewDir = -psInput.rayDir;
	int samples = lerp(SampleCountMax, SampleCountMin, viewDir.y);

	float dist0 = AltitudeMin / viewDir.y;
	float dist1 = AltitudeMax / viewDir.y;
	float stride = (dist1 - dist0) / samples;

	if (viewDir.y < 0.01 || dist0 >= FarDistance)
	{
		return float4(lerp(psInput.skyColor, psInput.groundColor, saturate(-viewDir.y)), 1.0);
	}

	float hg = HenyeyGreenstein(LightDir, viewDir, HGCoeff);

	float2 uv = psInput.uv;
	float offs = UVRandom(uv) * (dist1 - dist0) / samples;

	float3 posWS = ViewerPos + viewDir * (dist0 + offs);
	float3 acc = 0;

	float depth = 0;
	[loop]
	for (int s = 0; s < samples; ++s)
	{
		float n = SampleNoise(posWS);
		if (n > 0)
		{
			float density = n * stride;
			float rand = UVRandom(uv + s + 1);
			float scatter = density * Scatter * hg * MarchLight(posWS, rand * 0.5);
			//float scatter = 0.25f;
			acc += LightColor * scatter * BeerPowder(depth);
			depth += density;
		}
		posWS += viewDir * stride;
	}

	acc += Beer(depth) * psInput.skyColor;
	acc = lerp(acc, psInput.skyColor, saturate(dist0 / FarDistance));

	return float4(acc, 1.0);
	return float4(SampleCount / 2000, 0,0, 1.0);
}

