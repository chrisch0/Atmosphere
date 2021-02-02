#include "AtmosphereCommon.hlsli"

Texture2D<float4> Transmittance : register(t0);
Texture3D<float4> Scattering : register(t1);
Texture2D<float4> Irradiance_Texture : register(t2);
#ifdef RADIANCE_API_ENABLED
Texture3D<float4> SingleMieScattering : register(t3);
#endif 

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID )
{

}