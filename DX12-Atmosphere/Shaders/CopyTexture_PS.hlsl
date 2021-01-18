Texture2D<float3> ColorTex : register(t0);

struct PSInput
{
	float4 positionCS : SV_POSITION;
	float2 uv : TEXCOORD0;
};

float4 main(PSInput psInput) : SV_Target
{
	float3 linear_rgb = ColorTex[(int2)psInput.positionCS.xy];
	return float4(linear_rgb, 1.0);
}