#include "Common.hlsli"

Texture3D<float> Perlin : register(t0);
Texture3D<float> WorleyFBMLow : register(t1);
Texture3D<float> WorleyFBMMid : register(t2);
Texture3D<float> WorleyFBMHigh : register(t3);

RWTexture3D<float4> CloudBasicShape : register(u0);
RWTexture2D<float4> CloudBasicShapeR : register(u1);

SamplerState PointSampler : register(s0);
SamplerState LinearSampler : register(s1);

cbuffer Range
{
	float MinValue;
	float MaxValue;
};

[numthreads(8, 8, 8)]
void main( uint3 globalID : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex )
{
	float p = Perlin[globalID].x;
	float4 basic_shape;
	basic_shape.y = WorleyFBMLow[globalID].x;
	basic_shape.x = (p + basic_shape.y);

	basic_shape.x = (basic_shape.x - MinValue) / (MaxValue - MinValue);
	basic_shape.x = min(max(basic_shape.x, 0.0), 1.0);

	if (globalID.z == 0)
		CloudBasicShapeR[globalID.xy] = float4(basic_shape.xxx, 1.0);
	basic_shape.z = WorleyFBMMid[globalID].x;
	basic_shape.w = WorleyFBMHigh[globalID].x;
	CloudBasicShape[globalID] = basic_shape;

}