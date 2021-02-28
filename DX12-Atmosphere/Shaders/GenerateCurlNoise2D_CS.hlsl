#include "FastNoiseLite.hlsli"
RWTexture2D<float4> NoiseTexture : register(u0);

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
	float z_value;
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
void main( uint3 globalID : SV_DispatchThreadID )
{
	fnl_state noise_state = fnlCreateState(seed);
	float3 uvw = float3(globalID.xy, z_value);
	if (domain_warp_type > 0)
	{
		noise_state.domain_warp_type = domain_warp_type - 1;
		noise_state.frequency = domain_warp_frequency;
		noise_state.rotation_type_3d = domain_warp_rotation_type_3d;
		noise_state.domain_warp_amp = domain_warp_amp;
		noise_state.fractal_type = domain_warp_fractal_type == 0 ? domain_warp_fractal_type : domain_warp_fractal_type + 3;
		noise_state.octaves = domain_warp_octaves;
		noise_state.lacunarity = domain_warp_lacunarity;
		noise_state.gain = domain_warp_gain;

		fnlDomainWarp3D(noise_state, uvw.x, uvw.y, uvw.z);
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

	float val = fnlGetNoise3D(noise_state, uvw.x, uvw.y, uvw.z);
	float delta = 0.001f;
	float Px = fnlGetNoise3D(noise_state, uvw.x - delta, uvw.y, uvw.z);
	Px = (val - Px) / delta;
	float Py = fnlGetNoise3D(noise_state, uvw.x, uvw.y - delta, uvw.z);
	Py = (val - Py) / delta;
	float Pz = fnlGetNoise3D(noise_state, uvw.x, uvw.y, uvw.z - delta);
	Pz = (val - Pz) / delta;
	float3 curl = float3(Py - Pz, Pz - Px, Px - Py);
	//NoiseTexture[globalID.xy] = float4(val, val, val, 1.0);
	NoiseTexture[globalID.xy] = float4(curl, 1.0);
}