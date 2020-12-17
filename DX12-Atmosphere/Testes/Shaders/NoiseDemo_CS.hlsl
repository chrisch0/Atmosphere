#include "../../Shaders/FastNoiseLite.hlsli"

RWTexture2D<float4> noise : register(u0);
RWBuffer<int> gMinMax : register(u1);

cbuffer cb0
{
	int seed;
	float frequency;
};

groupshared int groupMinMax[2];

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
	if (groupIndex == 0)
	{
		groupMinMax[0] = 0;
		groupMinMax[1] = 0xff7fffff;
	}

	GroupMemoryBarrierWithGroupSync();

	fnl_state noise_state = fnlCreateState(seed);
	noise_state.frequency = frequency;

	float res = fnlGetNoise2D(noise_state, (float)globalID.x, (float)globalID.y);

	// assert min value < 0
	if (res < 0)
	{
		InterlockedMax(groupMinMax[0], asint(-res));
	}
	InterlockedMax(groupMinMax[1], asint(res));

	GroupMemoryBarrierWithGroupSync();

	if (groupIndex == 0)
	{
		InterlockedMax(gMinMax[0], groupMinMax[0]);
		InterlockedMax(gMinMax[1], groupMinMax[1]);
	}

	GroupMemoryBarrierWithGroupSync();

	res = (res + 1.0f) * 0.5f;

	noise[globalID.xy] = float4(res, res, res, 1.0);
}