#include "../../Shaders/FastNoiseLite.hlsli"

RWTexture3D<float4> noise3D : register(u0);

cbuffer cb0
{
	int seed;
	float frequency;
};

[numthreads(8, 8, 1)]
void main( uint3 threadID : SV_DispatchThreadID )
{
	fnl_state noise_state = fnlCreateState(seed);
	noise_state.frequency = frequency;

	float noise = fnlGetNoise3D(noise_state, (float)threadID.x, (float)threadID.y, (float)threadID.z);
	noise = (noise + 1.0f) * 0.5f;
	noise3D[threadID] = float4(noise, noise, noise, 1.0);
}