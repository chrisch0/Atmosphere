#include "../../Shaders/FastNoiseLite.hlsli"

RWTexture2D<float3> noise : register(u0);
RWStructuredBuffer<uint> gMinMax : register(u1);

cbuffer cb0
{
	int seed;
	float frequency;
};

groupshared uint groupMinMax[2];

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID, uint3 groupThreadID : SV_GroupThreadID )
{
	if (groupThreadID.x == 0 && groupThreadID.y == 0)
	{
		groupMinMax[0] = 1.0f;
		groupMinMax[1] = -1.0f;
	}

	GroupMemoryBarrierWithGroupSync();

	fnl_state noise_state = fnlCreateState(seed);
	noise_state.frequency = frequency;

	float res = fnlGetNoise2D(noise_state, globalID.x, globalID.y);

	uint res_i = asuint(res);

	InterlockedMin(groupMinMax[0], res_i);
	InterlockedMax(groupMinMax[1], res_i);

	GroupMemoryBarrierWithGroupSync();

	if (groupThreadID.x == 0 && groupThreadID.y == 0)
	{
		InterlockedMin(gMinMax[0], groupMinMax[0]);
		InterlockedMax(gMinMax[1], groupMinMax[1]);
	}

	GroupMemoryBarrierWithGroupSync();



	noise[globalID.xy] = float3(res, res, res);
}