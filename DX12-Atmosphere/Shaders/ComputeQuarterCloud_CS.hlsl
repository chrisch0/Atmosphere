#include "VolumetricCloudCommon.hlsli"

Texture3D<float4> CloudShapeTexture : register(t0);
Texture3D<float4> ErosionTexture : register(t1);
Texture2D<float4> WeatherTexture : register(t2);

RWTexture2D<float4> CloudColor : register(u0);

SamplerState LinearRepeatSampler : register(s0);

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float4 frag_color, bloom, alphaness, cloud_distance;
	float2 pixel_coord = float2(globalID.xy) + 0.5f;

}