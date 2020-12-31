RWTexture2D<float4> GradientTexture : register(u0);

cbuffer CloudRange : register(b0)
{
	float CloudMin;
	float CloudMax;
	float SolidCloudMin;
	float SolidCloudMax;
	float4 TextureSize;
};

[numthreads(1, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float height = (TextureSize.y - float(globalID.y) + 0.5f) / TextureSize.y;
	float gradient = 0.0f;

	if (height > CloudMin && height < SolidCloudMin)
		gradient = pow(smoothstep(0.0f, 1.0f, (height - CloudMin) / (SolidCloudMin - CloudMin)), 1.0f);
	else if (height > SolidCloudMin && height < SolidCloudMax)
		gradient = 1.0f;
	else if (height > SolidCloudMax && height < CloudMax)
		gradient = pow(1.0f - smoothstep(0.0f, 1.0f, (height - SolidCloudMax) / (CloudMax - SolidCloudMax)), 1.0f);
	
	GradientTexture[globalID.xy] = float4(gradient, gradient, gradient, 1.0f);
}