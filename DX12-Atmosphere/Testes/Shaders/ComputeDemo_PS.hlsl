struct PSIn
{
	float4 positionCS : SV_POSITION;
	float2 uv : TEXCOORD0;
};

SamplerState sampler0 : register(s0);
Texture2D<float3> noiseTex : register(t0);

float4 main(PSIn psIn) : SV_Target
{
	float3 n = noiseTex.Sample(sampler0, psIn.uv).xyz;
	float4 color = float4(n, 1.0);
	return color;
}