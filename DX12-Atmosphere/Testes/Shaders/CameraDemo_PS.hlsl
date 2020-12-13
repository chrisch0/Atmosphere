struct PSIn
{
	float4 positionCS : SV_POSITION;
	float4 color : COLOR;
};

float4 main(PSIn pixel) : SV_Target
{
	return pixel.color;
}