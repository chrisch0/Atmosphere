#include "FastNoiseLite.hlsli"

RWTexture2D<float4> noise_texture : register(u0);

cbuffer noise_state : register(b0)
{
	int seed;
	float frequency;
	int noise_type;
	int rotation_type_3d;
	// fractal parameters
	int fractal_type;
	int octaves;
	float lacunarity;
	float gain;
	float weighted_strength;
	float ping_pong_strength;
	// distance function
	int cellular_distance_func;
	int cellular_return_type;
	float cellular_jitter_mod;
	// domain warp
	int domain_warp_type;
	float domain_warp_amp;
	// is reverse color
	int invert;
};

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	fnl_state noise_state = fnlCreateState(seed);
	noise_state.frequency = frequency;
	noise_state.noise_type = noise_type;
	noise_state.rotation_type_3d = rotation_type_3d;
	noise_state.fractal_type = fractal_type;
	noise_state.octaves = octaves;
	noise_state.lacunarity = lacunarity;
	noise_state.gain = gain;
	noise_state.weighted_strength = weighted_strength;
	noise_state.ping_pong_strength = ping_pong_strength; 
	noise_state.cellular_distance_func = cellular_distance_func;
	noise_state.cellular_return_type = cellular_return_type;
	noise_state.cellular_jitter_mod = cellular_jitter_mod;
	noise_state.domain_warp_type = domain_warp_type;
	noise_state.domain_warp_amp = domain_warp_amp;

	float val = fnlGetNoise2D(noise_state, (float)globalID.x, (float)globalID.y);
	val = (val + 1.0f) * 0.5f;
	if (invert == 1)
	{
		val = 1.0 - val;
	}
	noise_texture[globalID.xy] = float4(val, val, val, 1.0);
}