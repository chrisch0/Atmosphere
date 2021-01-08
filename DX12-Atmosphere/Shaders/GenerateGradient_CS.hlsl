RWTexture2D<float4> GradientTexture : register(u0);

cbuffer CloudTypeParams : register(b0)
{
	float4 Stratus;
	float4 Cumulus;
	float4 Cumulonimbus;
	float4 TextureSize;
};

float GetDensityGradient(float4 params, float height)
{
	float gradient = 0.0f;
	if (height > params.x && height < params.y)
		gradient = pow(smoothstep(0.0f, 1.0f, (height - params.x) / (params.y - params.x)), 1.0f);
	else if (height > params.y && height < params.z)
		gradient = 1.0f;
	else if (height > params.z && height < params.w)
		gradient = pow(1.0f - smoothstep(0.0f, 1.0f, (height - params.z) / (params.w - params.z)), 1.0f);

	return gradient;
}

[numthreads(1, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float height = (float(globalID.y) + 0.5f) / TextureSize.y;
	float gradient = 0.0f;
	if (globalID.x <= 1)
		gradient = GetDensityGradient(Stratus, height);
	else if (globalID.x <= 3)
		gradient = GetDensityGradient(Cumulus, height);
	else
		gradient = GetDensityGradient(Cumulonimbus, height);
	GradientTexture[globalID.xy] = float4(gradient, gradient, gradient, 1.0f);
}