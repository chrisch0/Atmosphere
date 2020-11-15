struct VertexOut
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
};

float4 main(VertexOut pixel) : SV_Target
{
	return pixel.color;
}