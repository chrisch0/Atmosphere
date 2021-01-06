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
Texture2D<float4> StratusGradient : register(t1);
Texture2D<float4> CumulusGradient : register(t2);
Texture2D<float4> CumulonimbusGradient : register(t3);
Texture3D<float> PerlinNoise : register(t4);

SamplerState LinearRepeatSampler : register(s0);

struct PSInput
{
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
	float3 rayDir : TEXCOORD1;
	float3 skyColor : TEXCOORD2;
	float3 groundColor : TEXCOORD3;
};

//static const float Extinct = 0.0025f;
//static const float3 LightDir = float3(0.0f, 0.7071, -0.7071);
//static const int LightSampleCount = 16;
//static const float AltitudeMin = 1000;
//static const float AltitudeMax = 5000;
//static const float FarDistance = 22000;
//static const float HGCoeff = 0.5f;
//static const float Scatter = 0.009;
//static const float3 LightColor = float3(1.0, 0.9568627, 0.8392157);
static const float Freq1 = 1.34;
static const float Freq2 = 13.57;
static const float Amp1 = -8.5;
static const float Amp2 = 2.49;
static const float Bias = 2.19;

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

float SampleNoise(float3 posWS)
{
	const float base_freq = 1e-5;

	float3 uvw = posWS * base_freq;
	float base_cloud = BasicCloudShape.SampleLevel(LinearRepeatSampler, uvw, 0).r;

	/*float3 uvw1 = posWS * Freq1 * base_freq;
	float3 uvw2 = posWS * Freq2 * base_freq;

	float n1 = BasicCloudShape.SampleLevel(LinearRepeatSampler, uvw1, 0).g;
	float n2 = PerlinNoise.SampleLevel(LinearRepeatSampler, uvw2, 0);
	float base_cloud = n1 * Amp1 + n2 * Amp2;

	base_cloud = saturate(base_cloud + Bias);*/

	/*float3 uvw = posWS * base_freq;
	float4 low_frequency_noises = BasicCloudShape.SampleLevel(LinearRepeatSampler, uvw, 0);
	float low_freq_FBM =
		(low_frequency_noises.g * 0.625f) +
		(low_frequency_noises.b * 0.25f) +
		(low_frequency_noises.a * 0.125f);
	float base_cloud = Remap(low_frequency_noises.r, -(1.0f - low_freq_FBM), 1.0, 0.0, 1.0);*/

	float y = posWS.y - AltitudeMin;
	float h = AltitudeMax - AltitudeMin;
	base_cloud *= smoothstep(0, h * 0.1, y);
	base_cloud *= smoothstep(0, h * 0.4, h - y);
	return base_cloud;
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

	if (viewDir.y < 0.01)
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

