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
Texture3D<float> PerlinNoise : register(t4);
Texture2D<float3> WeatherTexture : register(t5);

SamplerState LinearRepeatSampler : register(s0);

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
	return DensityHeightGradient.SampleLevel(LinearRepeatSampler, float2(weatherData.g, height_fraction), 0).r;
}

float SampleNoise(float3 posWS)
{
	const float scale = 0.00005f;
	
	float3 uvw = (posWS) * scale;
	float4 low_frequency_noises = BasicCloudShape.SampleLevel(LinearRepeatSampler, uvw, 0);
	float low_freq_FBM =
		(low_frequency_noises.g * 0.625f) +
		(low_frequency_noises.b * 0.25f) +
		(low_frequency_noises.a * 0.125f);
	float base_cloud = Remap(low_frequency_noises.r, -(1.0f - low_freq_FBM), 1.0, 0.0, 1.0);
	
	float3 weather_data = GetWeatherData(posWS);
	float density_height_gradient = GetDensityHeightGradientForPoint(posWS, weather_data);
	
	return base_cloud * pow(weather_data.r, 2.0) * density_height_gradient;
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
}

