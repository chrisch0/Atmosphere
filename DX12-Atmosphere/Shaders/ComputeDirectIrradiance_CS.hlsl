#include "AtmosphereCommon.hlsli"

RWTexture2D<float4> InterIrradiance : register(u0);
RWTexture2D<float4> Irradiance_Texture : register(u1);

Texture2D<float4> Transmittance : register(t0);

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float2 pixel_coord = float2(globalID.xy) + 0.5;
	float3 inter_irradiance = ComputeDirectIrradianceTexture(Atmosphere, Transmittance, pixel_coord);
	float3 irradiance = 0.0;

	InterIrradiance[globalID.xy] = float4(inter_irradiance, 1.0);
	Irradiance_Texture[globalID.xy] = float4(irradiance, 0.0);
}