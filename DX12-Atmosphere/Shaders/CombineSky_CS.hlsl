
RWTexture2D<float4> PrevCloudColor : register(u0);
RWTexture2D<float4> CloudColor : register(u1);

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	PrevCloudColor[globalID.xy] = CloudColor[globalID.xy];
}