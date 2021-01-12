#include "ColorUtils.hlsli"

ByteAddressBuffer Histogram : register(t0);

RWStructuredBuffer<float> Exposure : register(u0);

cbuffer cb0 : register(b1)
{
	float TargetLuminance;
	float AdaptationRate;
	float MinExposure;
	float MaxExposure;
	uint PixelCount;
}

groupshared float GroupAccum[256];

[numthreads(256, 1, 1)]
void main( uint groupThreadID : SV_GroupIndex )
{
	// approximately equal to luminance(loged) * luminance_count
	float weighted_sum = (float)groupThreadID * (float)Histogram.Load(groupThreadID * 4);

	// calculate total sum
	// i = 1, 2, 4, 8, 16, 32, 64, 128
	[unroll]
	for (uint i = 1; i < 256; i *= 2)
	{
		GroupAccum[groupThreadID] = weighted_sum;
		GroupMemoryBarrierWithGroupSync();
		weighted_sum += GroupAccum[(groupThreadID + i) % 256];
		GroupMemoryBarrierWithGroupSync();
	}

	float min_log = Exposure[4];
	float max_log = Exposure[5];
	float log_range = Exposure[6];
	float rcp_log_range = Exposure[7];

	// Average histogram value is the weighted sum of all pixels divided by the total number of pixels
	// minus those pixels which provided no weight (i.e. black pixels.)
	float weighted_hist_avg = weighted_sum / (max(1, PixelCount - Histogram.Load(0))) - 1.0;
	// convert back to luminance
	float log_avg_luminance = exp2(weighted_hist_avg / 254.0 * log_range + min_log);
	float target_exposure = TargetLuminance / log_avg_luminance;

	float exposure = Exposure[0];
	exposure = lerp(exposure, target_exposure, AdaptationRate);
	exposure = clamp(exposure, MinExposure, MaxExposure);

	if (groupThreadID == 0)
	{
		Exposure[0] = exposure;
		Exposure[1] = 1.0 / exposure;
		Exposure[2] = exposure;
		Exposure[3] = weighted_hist_avg;

		// First attempt to recenter our histogram around the log-average.
		float bias_to_center = (floor(weighted_hist_avg) - 128.0) / 255.0;
		if (abs(bias_to_center) > 0.1)
		{
			min_log += bias_to_center * rcp_log_range;
			max_log += bias_to_center * rcp_log_range;
		}

		// TODO:  Increase or decrease the log range to better fit the range of values.
		// (Idea) Look at intermediate log-weighted sums for under- or over-represented
		// extreme bounds.  I.e. break the for loop into two pieces to compute the sum of
		// groups of 16, check the groups on each end, then finish the recursive summation.

		Exposure[4] = min_log;
		Exposure[5] = max_log;
		Exposure[6] = log_range;
		Exposure[7] = 1.0 / log_range;
	}
}