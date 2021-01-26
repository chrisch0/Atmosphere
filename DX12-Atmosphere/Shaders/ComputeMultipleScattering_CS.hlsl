#include "AtmosphereCommon.hlsli"

RWTexture3D<float4> InterMultipleScattering : register(u0);
RWTexture3D<float4> Scattering : register(u1);

Texture2D<float4> Transmittance : register(t0);
Texture3D<float4> ScatteringDensity : register(t1);

cbuffer cb : register(b0)
{
	float3x3 LuminanceFromRadiance;
	int ScatteringOrder;
};

[numthreads(8, 8, 8)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float nu;
	float3 pixel_coord = float3(globalID) + 0.5;
	float3 inter_multiple_scattering = ComputeMultipleScatteringTexture(Atmosphere, Transmittance, ScatteringDensity, pixel_coord, nu);
	float4 scattering = float4(mul(LuminanceFromRadiance, inter_multiple_scattering) / RayleighPhaseFunction(nu), 0.0);

	scattering += Scattering[globalID];

	InterMultipleScattering[globalID] = float4(inter_multiple_scattering, 1.0);
	Scattering[globalID] = scattering;
}