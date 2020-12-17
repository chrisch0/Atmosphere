struct PSIn
{
	float4 positionCS : SV_POSITION;
	float2 uv : TEXCOORD0;
};

SamplerState sampler0 : register(s0);
Texture3D<float3> noiseTex3D : register(t0);

float4 main(PSIn psIn) : SV_Target
{
	float2 uv = frac(psIn.uv * 16.0f);
	float w = floor(psIn.uv.y * 16.0f) * 16.0f + floor(psIn.uv.x * 16.0f);
	w /= 256.0f;
	float3 n = noiseTex3D.Sample(sampler0, float3(uv, w)).xyz;
	float4 color = float4(n, 1.0);
	return color;
}