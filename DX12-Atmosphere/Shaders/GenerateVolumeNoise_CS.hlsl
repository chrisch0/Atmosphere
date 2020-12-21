#include "FastNoiseLite.hlsli"

RWTexture3D<float4> noise_volume_texture : register(u0);

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
	int invert;
	// domain warp parameters
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
void main(uint3 globalID : SV_DispatchThreadID)
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

	float val = fnlGetNoise3D(noise_state, (float)globalID.x, (float)globalID.y, (float)globalID.z);
	val = (val + 1.0f) * 0.5f;
	if (invert == 1)
	{
		val = 1.0 - val;
	}
	noise_volume_texture[globalID.xyz] = float4(val, val, val, 1.0);
}