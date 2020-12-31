cbuffer PassConstant : register(b0)
{
	float4x4 ViewMatrix;
	float4x4 ProjMatrix;
	float4x4 ViewProjMatrix;
	float4x4 InvViewMatrix;
	float4x4 InvProjMatrix;
	float4x4 InvViewProjMatrix;
	float3 ViewerPos;
}

Texture3D<float4> BasicCloudShape : register(t0);
Texture2D<float4> StratusGradient : register(t1);
Texture2D<float4> CumulusGradient : register(t2);
Texture2D<float4> CumulonimbusGradient : register(t3);

SamplerState LinearSampler : register(s0);

struct PSInput
{
	float4 position : SV_Position;
	float3 worldPos : WorldPos;
	float2 uv : Texcoord0;
	float3 viewDir : Texcoord1;
	float3 boxCenter : Texcoord2;
	float3 boxScale : Texcoord3;
	float3 boxMinWorld : Texcoord4;
	float3 boxMaxWorld : Texcorrd5;
};

float GetHeightFractionForPoint(float3 position, float2 cloudMinMax)
{
	float height_fraction = (position.y - cloudMinMax.x) / (cloudMinMax.y - cloudMinMax.x);
	return saturate(height_fraction);
}

float GetDensityHeightGradientForPoint(float3 position) // float3 weatherData
{
	float height_fraction = GetHeightFractionForPoint(position, float2(0.0f, 1.0f));
	return CumulusGradient.Sample(LinearSampler, float2(0.5f, 1.0f - height_fraction));
}

void CalculateRaymarchStartEnd(float3 viewerPos, float3 dir, float3 boxMin, float3 boxMax, out float3 start, out float3 end)
{
	float3 inv_dir = 1.0f / dir;
	float3 p0 = (boxMin - viewerPos) * inv_dir;
	float3 p1 = (boxMax - viewerPos) * inv_dir;
	float3 t0 = min(p0, p1);
	float3 t1 = max(p0, p1);

	float t_min = max(t0.x, max(t0.y, t0.z));
	float t_max = min(t1.x, min(t1.y, t1.z));
	
	start = viewerPos + dir * t_min;
	end = viewerPos + dir * t_max;
}

float3 LocalPositionToUVW(float3 p, float3 boxScale)
{
	float3 box_min = float3(-0.5f, -0.5f, -0.5f) * boxScale;
	return (p - box_min) / boxScale;
}

float Remap(float originalValue, float originalMin, float originalMax,
	float newMin, float newMax)
{
	return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
}

float SampleCloudDensity(float3 p, float3 weather_data, bool simpleSample)
{
	float4 low_frequency_noises = BasicCloudShape.Sample(LinearSampler, p).rgba;
	float low_freq_FBM = 
		(low_frequency_noises.g * 0.625f) +
		(low_frequency_noises.b * 0.25f) +
		(low_frequency_noises.a * 0.125f);
	float base_cloud = Remap(low_frequency_noises.r, -(1.0f - low_freq_FBM), 1.0, 0.0, 1.0);
	float density_height_gradient = GetDensityHeightGradientForPoint(p);
	
	//base_cloud *= density_height_gradient;
	
	return base_cloud * density_height_gradient;
}

float4 main(PSInput psInput) : SV_Target
{
	float3 raymarch_start = ViewerPos + psInput.viewDir;
	float3 dir = normalize(psInput.viewDir);

	const int sample_count = 64;
	float3 start = 0.0f;
	float3 end = 0.0f;
	CalculateRaymarchStartEnd(ViewerPos, dir, psInput.boxMinWorld, psInput.boxMaxWorld, start, end);
	float step = length(end - start) / sample_count;

	float density = 0.0f;
	float3 p = start;
	for (int i = 0; i < sample_count; ++i)
	{
		float3 uvw = LocalPositionToUVW(p, psInput.boxScale);
		density += SampleCloudDensity(uvw, float3(1.0f, 1.0f, 1.0f), false) * step;
		p += dir * step;

	}
	//density = exp(-density);
	density *= 1.2f;

	return float4(density, density, density, 1.0f);
}