
struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv  : TEXCOORD0;
};

cbuffer VolumeTextureDim : register(b1)
{
	int dimensionZ;
};

SamplerState sampler0 : register(s0);
Texture3D<float4> tex3D : register(t0);

float4 main(PS_INPUT input) : SV_Target
{
	float4 out_col = input.col * tex3D.Sample(sampler0, float3(input.uv, 0.5 / float(dimensionZ)));
	return out_col;
}