cbuffer PassConstants : register(b0)
{
	float3 ViewerPos;
	float Time;
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

static const float Extinct = 0.0025f;
static const float3 LightDir = float3(0.0f, -1.0f, 1.0f);
static const int LightSampleCount = 16;
static const float AltitudeMin = 1500;
static const float AltitudeMax = 3500;
static const float FarDistance = 30000;

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
	return exp(-Extinct * depth) * (1 - exp(-Extinct * 2 * depth));
}

float SampleNoise(float3 posWS)
{
	const float base_freq = 1e-5;
	float3 uvw = posWS * base_freq;

}

float MarchLight(float3 posWS, float rand)
{
	float stride = (AltitudeMax - posWS.y) / (LightDir.y * LightSampleCount);
	posWS += LightDir * stride * rand;

	float depth = 0;
	[loop]
	for (int s = 0; s < LightSampleCount; ++s)
	{
		depth += SampleNoise(posWS) * stride;
		posWS 
	}
}

float4 main(PSInput psInput) : SV_Target
{
	float3 viewDir = -psInput.rayDir;
	if (viewDir.y < 0.01)
	{
		return float4(lerp(psInput.skyColor, psInput.groundColor, saturate(-viewDir.y)), 1.0);
	}
	return float4(psInput.skyColor, 1.0);
}

