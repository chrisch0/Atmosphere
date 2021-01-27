#include "AtmosphereCommon.hlsli"

RWTexture3D<float4> ScatteringDensity : register(u0);

Texture2D<float4> Transmittance : register(t0);
Texture3D<float4> SingleRayleighScattering : register(t1);
Texture3D<float4> SingleMieScattering : register(t2);
Texture3D<float4> MultipleScattering : register(t3);
Texture2D<float4> Irradiance_Texture : register(t4);

cbuffer SO : register(b0)
{
	int ScatteringOrder;
}

[numthreads(8, 8, 8)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float3 pixel_coord = float3(globalID) + 0.5f;
	float3 scattering_density = ComputeScatteringDensityTexture(
		Atmosphere, Transmittance,
		SingleRayleighScattering, SingleMieScattering, MultipleScattering, Irradiance_Texture,
		pixel_coord, ScatteringOrder);
	ScatteringDensity[globalID] = float4(scattering_density, 1.0);
}