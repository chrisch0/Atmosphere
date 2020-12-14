struct VSIn
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD;
	float3 color : COLOR;
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
	//float4x4 MVP = ViewProjMatrix * ModelMatrix;
	float4x4 MVP = mul(ViewProjMatrix, ModelMatrix);
	vsOut.positionCS = mul(MVP, float4(vert.position, 1.0));
	vsOut.color = float4(vert.color, 1.0);

	return vsOut;
}