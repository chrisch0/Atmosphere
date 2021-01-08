#include "Common.hlsli"

Texture3D<float> Perlin : register(t0);
Texture3D<float> WorleyFBMLow : register(t1);
Texture3D<float> WorleyFBMMid : register(t2);
Texture3D<float> WorleyFBMHigh : register(t3);

RWTexture3D<float4> CloudBasicShape : register(u0);
RWTexture2D<float4> CloudBasicShapeR : register(u1);

SamplerState PointRepeatSampler : register(s0);
SamplerState LinearRepeatSampler : register(s1);

cbuffer CloudShapeSetting : register(b0)
{
	float MinValue;
	float MaxValue;
	float PerlinSampleFreq;
	float WorleySampleFreq;
	float PerlinAmp;
	float WorleyAmp;
	float NoiseBias;
	int Method2;
	float3 TextureSize;
};

float Remap(float originalValue, float originalMin, float originalMax,
	float newMin, float newMax)
{
	return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
}

[numthreads(8, 8, 8)]
void main( uint3 globalID : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex )
{
	float p = Perlin[globalID].x;
	float4 basic_shape;
	basic_shape.y = WorleyFBMLow[globalID].x;
	if (Method2)
	{
		float3 uvw = (float3(globalID)+0.5) / TextureSize;
		float3 worley_uvw = uvw * WorleySampleFreq;
		float3 perlin_uvw = uvw * PerlinSampleFreq;
		
		float worley = WorleyFBMLow.SampleLevel(LinearRepeatSampler, worley_uvw, 0);
		float perlin = Perlin.SampleLevel(LinearRepeatSampler, perlin_uvw, 0);
		float base_cloud = worley * WorleyAmp + perlin * PerlinAmp;
		base_cloud = saturate(base_cloud + NoiseBias);
		basic_shape.x = base_cloud;
	}
	else
	{
		float w = basic_shape.y;
		//basic_shape.x = (p - w) / max(0.1, (1.0 - w));

		basic_shape.x = w - p * (1.0 - w);
		basic_shape.x = saturate(basic_shape.x + MinValue);

		//basic_shape.x = (basic_shape.x - MinValue) / (MaxValue - MinValue);
		/*basic_shape.x = (p + basic_shape.y);
		basic_shape.x = (basic_shape.x - MinValue) / (MaxValue - MinValue);
		basic_shape.x = min(max(basic_shape.x, 0.0), 1.0);*/
	}

	if (globalID.z == 0)
		CloudBasicShapeR[globalID.xy] = float4(basic_shape.xxx, 1.0);
	basic_shape.z = WorleyFBMMid[globalID].x;
	basic_shape.w = WorleyFBMHigh[globalID].x;
	CloudBasicShape[globalID] = basic_shape;

}