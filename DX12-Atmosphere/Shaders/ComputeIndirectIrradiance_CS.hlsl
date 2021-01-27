#include "AtmosphereCommon.hlsli"

RWTexture2D<float4> InterIrradiance : register(u0);
RWTexture2D<float4> Irradiance_Texture : register(u1);

Texture3D<float4> SingleRayleighScattering : register(t0);
Texture3D<float4> SingleMieScattering : register(t1);
Texture3D<float4> MultipleScattering : register(t2); 

cbuffer SO : register(b0)
{
	int ScatteringOrder;
};

cbuffer Convert : register(b2)
{
	float3x3 LuminanceFromRadiance;
}

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float2 pixel_coord = float2(globalID.xy) + 0.5;
	float3 inter_irradiance = ComputeIndirectIrradianceTexture(Atmosphere, SingleRayleighScattering, SingleMieScattering, MultipleScattering, pixel_coord, ScatteringOrder - 1);
	float3 irradiance = mul(LuminanceFromRadiance, inter_irradiance);

	irradiance += Irradiance_Texture[globalID.xy].xyz;

	Irradiance_Texture[globalID.xy] = float4(irradiance, 1.0);
	InterIrradiance[globalID.xy] = float4(inter_irradiance, 1.0);
}