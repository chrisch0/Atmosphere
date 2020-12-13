struct VSIn
{
	float3 position : POSITION;
};

struct VSOut
{
	float4 positionCS : SV_POSITION;
	float4 color : COLOR;
};

cbuffer ObjectConstant : register(b0)
{
	float4x4 ModelMatrix;
	float4 ModelColor;
};

cbuffer PassConstant : register(b1)
{
	float4x4 ViewMatrix;
	float4x4 ProjMatrix;
	float4x4 ViewProjMatrix;
	float4x4 InvViewMatrix;
	float4x4 InvProjMatrix;
	float4x4 InvViewProjMatrix;
}

VSOut main(VSIn vert)
{
	VSOut vsOut;
	float4x4 MVP = ViewProjMatrix * ModelMatrix;
	vsOut.positionCS = mul(MVP, float4(vert.position, 1.0));
	vsOut.color = ModelColor;

	return vsOut;
}