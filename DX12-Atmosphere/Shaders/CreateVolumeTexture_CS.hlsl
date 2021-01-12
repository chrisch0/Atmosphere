cbuffer Slice
{
	int4 SliceInfo;
};

Texture2D<float4> InputTexture : register(t0);

RWTexture3D<float4> VolumeTexture : register(u0);

[numthreads(8, 8, 8)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	uint2 uv = uint2(globalID.x + (globalID.z % SliceInfo.z) * SliceInfo.x, globalID.y + (globalID.z / SliceInfo.z) * SliceInfo.y);
	VolumeTexture[globalID] = InputTexture[uv];
}