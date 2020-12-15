struct VSIn
{
	float3 positionLS : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD0;
};

struct VSOut
{
	float4 positionCS : SV_POSITION;
	float2 uv : TEXCOORD0;
};

VSOut main(VSIn vertex)
{
	VSOut pos;
	pos.uv = vertex.uv;
	pos.positionCS = float4(vertex.positionLS, 1.0);
	return pos;
}