#include "ColorUtils.hlsli"
#include "Common.hlsli"

RWTexture2D<float4> NoiseTexture : register(u0);

Buffer<uint> MinMax : register(t0);

cbuffer ViewState
{
	int InverseVisualizeWarp;
};

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float3 val = NoiseTexture[globalID.xy].xyz;
	float min_val = ComparableUintToFloat(MinMax[0]);
	float max_val = ComparableUintToFloat(MinMax[1]);
	bool visualize_warp = (InverseVisualizeWarp & 1) > 0;
	if (visualize_warp)
	{
		val = (val - min_val) / (max_val - min_val) - 0.5f;

		float h = atan2(val.y, val.x) * 57.2957795 + 180.0;
		float b = min(1.0, sqrt(val.x * val.x + val.y * val.y) * 2.0);
		float s = 0.9f;

		val = HSVToRGB(float3(h, s, b));
	}
	else
		val = (val - min_val) / (max_val - min_val);

	bool invert = ((InverseVisualizeWarp >> 16) & 1) > 0;
	val = invert ? 1.0f - val : val;
	NoiseTexture[globalID.xy] = float4(val, 1.0);
}