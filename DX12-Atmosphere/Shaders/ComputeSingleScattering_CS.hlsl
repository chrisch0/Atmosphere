#include "AtmosphereCommon.hlsli"

RWTexture3D<float4> InterRayleigh : register(u0);
RWTexture3D<float4> InterMie : register(u1);
RWTexture3D<float4> Scattering : register(u2);
RWTexture3D<float4> SingleMieScattering : register(u3);

cbuffer cb : register(b0)
{
	float3x3 LuminanceFromRadiance;
	int ScatteringOrder;
};

Texture2D<float4> Transmittance : register(t0);

[numthreads(8, 8, 8)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float3 pixel_coord = float3(globalID.xyz) + 0.5;
	float3 inter_rayleigh;
	float3 inter_mie;
	ComputeSingleScatteringTexture(Atmosphere, Transmittance, pixel_coord, inter_rayleigh, inter_mie);
	float4 scattering = float4(mul(LuminanceFromRadiance, inter_rayleigh), mul(LuminanceFromRadiance, inter_mie).r);
	float3 single_mie_scattering = mul(LuminanceFromRadiance, inter_mie);
	
	InterRayleigh[globalID] = float4(inter_rayleigh, 1.0);
	InterMie[globalID] = float4(inter_mie, 1.0);
	Scattering[globalID] = scattering;
	SingleMieScattering[globalID] = float4(single_mie_scattering, 0.0);
}