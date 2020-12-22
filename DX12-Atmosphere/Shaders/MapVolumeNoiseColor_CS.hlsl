RWTexture3D<float4> noise_volume_texture : register(u0);
Buffer<int> min_max : register(t0);

cbuffer InverseVisualizeWarp
{
	int invert_visualize_warp;
};

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float3 val = noise_volume_texture[globalID.xyz].xyz;
	float min_val = asfloat(min_max[0]);
	float max_val = asfloat(min_max[1]);

	val = (val + min_val) / (min_val + max_val);

	bool invert = (invert_visualize_warp >> 16) & 1 > 0;
	val = invert ? val : 1.0f - val;
	noise_volume_texture[globalID.xyz] = float4(val, 1.0);
}