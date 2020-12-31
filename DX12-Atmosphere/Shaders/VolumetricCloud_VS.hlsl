cbuffer ObjectConstants : register(b0)
{
	float4x4 MVPMatrix;
	float4x4 ModelMatrix;
	float3 ModelScale;
	float3 ViewerPos;
}

struct VSInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD0;
};

struct VSOutput
{
	float4 position : SV_Position;
	float3 worldPos : WorldPos;
	float2 uv : Texcoord0;
	float3 viewDir : Texcoord1;
	float3 boxCenter : Texcoord2;
	float3 boxScale : Texcoord3;
	float3 boxMinWorld : Texcoord4;
	float3 boxMaxWorld : Texcorrd5;
};

VSOutput main(VSInput vsInput)
{
	VSOutput vs_output;
	vs_output.position = mul(MVPMatrix, float4(vsInput.position, 1.0f));
	vs_output.worldPos = mul(ModelMatrix, float4(vsInput.position, 1.0f)).xyz;
	vs_output.uv = vsInput.uv;
	vs_output.viewDir = vs_output.worldPos - ViewerPos;
	vs_output.boxCenter = float3(ModelMatrix[0].w, ModelMatrix[1].w, ModelMatrix[2].w);
	vs_output.boxScale = ModelScale;
	vs_output.boxMinWorld = mul(ModelMatrix, float4(-0.5f, -0.5f, -0.5f, 1.0f)).xyz;
	vs_output.boxMaxWorld = mul(ModelMatrix, float4(0.5f, 0.5f, 0.5f, 1.0f)).xyz;
	return vs_output;
}