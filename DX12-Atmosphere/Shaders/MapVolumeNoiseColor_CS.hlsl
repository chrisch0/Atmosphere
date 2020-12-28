#include "Common.hlsli"

RWTexture3D<float4> NoiseVolumeTexture : register(u0);
Buffer<uint> MinMax : register(t0);

cbuffer ViewState
{
	int InverseVisualizeWarp;
};

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float3 val = NoiseVolumeTexture[globalID.xyz].xyz;
	float min_val = ComparableUintToFloat(MinMax[0]);
	float max_val = ComparableUintToFloat(MinMax[1]);

	val = (val - min_val) / (max_val - min_val);

	bool invert = ((InverseVisualizeWarp >> 16) & 1) > 0;
	val = invert ? 1.0 - val : val;
	NoiseVolumeTexture[globalID.xyz] = float4(val, 1.0);
}