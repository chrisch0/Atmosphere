ByteAddressBuffer Histogram : register(t0);
StructuredBuffer<float> Exposure : register(t1);
RWTexture2D<float4> HistogramColorBuffer : register(u0);

groupshared uint GroupHist[256];

[numthreads(256, 1, 1)]
void main( uint groupThreadID : SV_GroupIndex, uint3 globalID : SV_DispatchThreadID )
{
	uint hist_value = Histogram.Load(groupThreadID * 4);

	GroupHist[groupThreadID] = groupThreadID == 0 ? 0 : hist_value;
	GroupMemoryBarrierWithGroupSync();

	[unroll]
	for (int offset = 1; offset < 256; offset *= 2)
	{
		GroupHist[groupThreadID] = max(GroupHist[groupThreadID], GroupHist[(groupThreadID + offset) % 256]);
		GroupMemoryBarrierWithGroupSync();
	}

	uint max_hist_value = GroupHist[groupThreadID];

	const uint2 group_corner = globalID.xy * 4;

	uint height = 127 - globalID.y * 4;
	uint threshold = hist_value * 128 / max(1, max_hist_value);

	float4 color = (groupThreadID == (uint)Exposure[3]) ? float4(1.0, 1.0, 0.0, 1.0) : float4(0.5, 0.5, 0.5, 1.0);

	[unroll]
	for (uint i = 0; i < 4; ++i)
	{
		float4 masked_color = (height - i) < threshold ? color : float4(0.0, 0.0, 0.0, 1.0);

		HistogramColorBuffer[group_corner + uint2(0, i)] = masked_color;
		HistogramColorBuffer[group_corner + uint2(1, i)] = masked_color;
		HistogramColorBuffer[group_corner + uint2(2, i)] = float4(0.0, 0.0, 0.0, 1.0);
		HistogramColorBuffer[group_corner + uint2(3, i)] = float4(0.0, 0.0, 0.0, 1.0);
	}
}