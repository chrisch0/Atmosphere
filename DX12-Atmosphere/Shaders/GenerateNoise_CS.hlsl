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
	// high 16 bit: is invert color, low 16 bit: is visualize domain warp
	int invert_visualize_warp;
	// domain warp parameters
	int domain_warp_seed;
	// minus 1 to use the actual domain warp type
	int domain_warp_type;
	int domain_warp_rotation_type_3d;
	float domain_warp_amp;
	float domain_warp_frequency;
	int domain_warp_fractal_type;
	int domain_warp_octaves;
	float domain_warp_lacunarity;
	float domain_warp_gain;
};

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	fnl_state noise_state = fnlCreateState(seed);
	float2 uv = float2(globalID.xy);
	if (domain_warp_type > 0)
	{
		noise_state.domain_warp_type = domain_warp_type - 1;
		noise_state.seed = domain_warp_seed;
		noise_state.frequency = domain_warp_frequency;
		noise_state.rotation_type_3d = domain_warp_rotation_type_3d;
		noise_state.domain_warp_amp = domain_warp_amp;
		noise_state.fractal_type = domain_warp_fractal_type;
		noise_state.octaves = domain_warp_octaves;
		noise_state.lacunarity = domain_warp_lacunarity;
		noise_state.gain = domain_warp_gain;

		fnlDomainWarp2D(noise_state, uv.x, uv.y);
	}
	bool visualize_warp = ((invert_visualize_warp & 1) > 0);
	if (visualize_warp)
	{

	}
	else
	{

	}
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


	float val = fnlGetNoise2D(noise_state, (float)globalID.x, (float)globalID.y);
	val = (val + 1.0f) * 0.5f;


	val = ((invert_visualize_warp & 0x00010000) > 0) ? 1.0 - val : val;
	noise_texture[globalID.xy] = float4(val, val, val, 1.0);
}