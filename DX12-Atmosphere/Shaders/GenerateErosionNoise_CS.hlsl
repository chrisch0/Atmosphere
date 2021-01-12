Texture3D<float> WorleyFBMLow : register(t0);
Texture3D<float> WorleyFBMMid : register(t1);
Texture3D<float> WorleyFBMHigh : register(t2);

RWTexture3D<float4> ErosionTexture : register(u0);

[numthreads(8, 8, 8)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float3 erosion;
	float worley_low = WorleyFBMLow[globalID];
	float worley_mid = WorleyFBMMid[globalID];
	float worley_high = WorleyFBMHigh[globalID];
	erosion = float3(worley_low, worley_mid, worley_high);
	ErosionTexture[globalID] = float4(erosion, 1.0);
}