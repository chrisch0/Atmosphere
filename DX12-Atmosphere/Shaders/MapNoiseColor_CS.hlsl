#include "ColorUtils.hlsli"

RWTexture2D<float4> noise_texture : register(u0);
Buffer<int> min_max : register(t0);

cbuffer InverseVisualizeWarp
{
	int invert_visualize_warp;
};

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float3 val = noise_texture[globalID.xy].xyz;
	float min_val = asfloat(min_max[0]);
	float max_val = asfloat(min_max[1]);
	bool visualize_warp = invert_visualize_warp & 1 > 0;
	if (visualize_warp)
	{
		val = (val + min_val) / (min_val + max_val) - 0.5f;

		float h = atan2(val.y, val.x) * 57.2957795 + 180.0;
		float b = min(1.0, sqrt(val.x * val.x + val.y * val.y) * 2.0);
		float s = 0.9f;

		val = HSVToRGB(float3(h, b, s));
	}
	else
		val = (val + min_val) / (min_val + max_val);

	bool invert = (invert_visualize_warp >> 16) & 1 > 0;
	val = invert ? val : 1.0f - val;
	noise_texture[globalID.xy] = float4(val, 1.0);
}