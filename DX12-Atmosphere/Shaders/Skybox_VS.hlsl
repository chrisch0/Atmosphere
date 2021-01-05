struct VSInput
{
	float3 position : POSITION;
};

struct VSOutput
{
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
	float3 rayDir : TEXCOORD1;
	float3 skyColor : TEXCOORD2;
	float3 groundColor : TEXCOORD3;
};

cbuffer ObjectConstant : register(b0)
{
	float4x4 ModelMatrix;
	float4x4 MVPMatirx;
};

VSOutput main(VSInput vsInput)
{
	VSOutput vsout;
	vsout.position = mul(MVPMatirx, float4(vsInput.position, 1.0));
	// screen space uv
	vsout.uv = (vsout.position.xy / vsout.position.w + 1) * 0.5f;
	vsout.rayDir = -normalize(mul(ModelMatrix, float4(vsInput.position, 0.0)));
	vsout.groundColor = float3(0.369, 0.349, 0.341);
	vsout.skyColor = float3(0.4851, 0.5986, 0.7534);
	return vsout;
}