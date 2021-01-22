#include "AtmosphereCommon.hlsli"

RWTexture2D<float4> Transmittance : register(u0);

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float2 pixelCoord = float2(globalID.xy) + 0.5;
	float3 pixelColor = ComputeTransmittanceToTopAtmosphereBoundaryTexture(Atmosphere, pixelCoord);
	Transmittance[globalID.xy] = float4(pixelColor, 1.0);
}