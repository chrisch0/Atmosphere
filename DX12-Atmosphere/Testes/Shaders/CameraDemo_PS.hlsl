struct PSIn
{
	float4 positionCS : SV_POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD0;
};

float4 main(PSIn pixel) : SV_Target
{
	float p = (frac(pixel.uv.x * 10.0) > 0.5) ^ (frac(pixel.uv.y * 10.0) < 0.5);
	return float4(p, p, p, 1.0);
}