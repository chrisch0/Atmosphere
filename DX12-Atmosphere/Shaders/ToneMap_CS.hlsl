#include "ColorUtils.hlsli"

StructuredBuffer<float> Exposure : register(t0);
RWTexture2D<float4> SceneColor : register(u0);

cbuffer cb0 : register(b0)
{
	float2 RcpBufferDim;
}

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float3 hdr_color = SceneColor[globalID.xy].xyz;
	hdr_color *= Exposure[0];

	float3 sdrColor = TM_Stanard(hdr_color);
	SceneColor[globalID.xy] = float4(sdrColor, 1.0);
}