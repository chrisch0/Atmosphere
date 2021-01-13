#include "ColorUtils.hlsli"

SamplerState BiLinearClamp : register(s0);

Texture2D<float3> SourceTex : register(t0);
StructuredBuffer<float> Exposure : register(t1);

RWTexture2D<uint> LumaResult : register(u0);

cbuffer cb0 : register(b0)
{
	float2 InverseOutputSize;
};

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float2 uv = globalID.xy * InverseOutputSize;
	float2 offset = InverseOutputSize * 0.25f;

	float3 color1 = SourceTex.SampleLevel(BiLinearClamp, uv + float2(-offset.x, -offset.y), 0);
	float3 color2 = SourceTex.SampleLevel(BiLinearClamp, uv + float2(offset.x, -offset.y), 0);
	float3 color3 = SourceTex.SampleLevel(BiLinearClamp, uv + float2(-offset.x, offset.y), 0);
	float3 color4 = SourceTex.SampleLevel(BiLinearClamp, uv + float2(offset.x, offset.y), 0);

	float luma = RGBToLuminance(color1 + color2 + color3 + color4) * 0.25f;

	if (luma == 0.0)
	{
		LumaResult[globalID.xy] = 0;
	}
	else
	{
		const float min_log = Exposure[4];
		const float rcp_log_range = Exposure[7];
		// remap luminance to log range
		float log_luma = saturate((log2(luma) - min_log) * rcp_log_range);
		LumaResult[globalID.xy] = log_luma * 254.0f + 1.0f;
	}
}