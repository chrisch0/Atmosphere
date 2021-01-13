#include "ColorUtils.hlsli"

Texture2D<float3> ColorTex : register(t0);

struct PSInput
{
	float4 positionCS : SV_POSITION;
	float2 uv : TEXCOORD0;
};

float4 main(PSInput psInput) : SV_Target
{
	float3 linear_rgb = RemoveDisplayProfile(ColorTex[(int2)psInput.positionCS.xy], LDR_COLOR_FORMAT);
	return float4(ApplyDisplayProfile(linear_rgb, DISPLAY_PLANE_FORMAT), 1.0);
}