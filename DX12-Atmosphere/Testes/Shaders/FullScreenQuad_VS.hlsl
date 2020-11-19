cbuffer cbGlobal : register(b0)
{
	float3 gColor;
	bool gUVAsColor;
}

struct VertexIn
{
	float3 pos : POSITION;
	float2 uv : TEXCOORD0;
};

struct VertexOut
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
};

VertexOut main(VertexIn vertex)
{
	VertexOut vsout;
	vsout.pos = float4(vertex.pos, 1.0);
	if (gUVAsColor)
	{
		vsout.color = float4(vertex.uv, 0.0, 1.0);
	}
	else
	{
		vsout.color = float4(gColor, 1.0);
	}

	return vsout;
}