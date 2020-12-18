
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
Texture3D<float3> tex3D : register(t0);

float4 main(PS_INPUT input) : SV_Target
{
	int y = sqrt(dimensionZ);
	int highbit = firstbithigh(y);
	y = 1 << highbit;
	int x = dimensionZ / y;
	float2 uv = frac(input.uv * float2(x, y));
	float w = floor(input.uv.y * float(y)) * float(x) + floor(input.uv.x * float(x));
	w = (w + 0.5) / float(dimensionZ);
	float3 n = tex3D.Sample(sampler0, float3(uv, w)).xyz;
	float4 color = float4(n, 1.0);
	float4 out_col = input.col * color;
	return out_col;
}