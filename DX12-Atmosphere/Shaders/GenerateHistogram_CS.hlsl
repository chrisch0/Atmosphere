Texture2D<uint> LumaBuf : register(t0);
RWByteAddressBuffer Histogram : register(u0);

groupshared uint GroupTileHistogram[256];

[numthreads(16, 16, 1)]
void main( uint groupThreadID : SV_GroupIndex, uint3 globalID : SV_DispatchThreadID )
{
	GroupTileHistogram[groupThreadID] = 0;
	GroupMemoryBarrierWithGroupSync();

	// Loop 24 times until the entire column has been processed
	for (uint top_y = 0; top_y < 384; top_y += 16)
	{
		uint quantized_log_luma = LumaBuf[globalID.xy + uint2(0, top_y)];
		InterlockedAdd(GroupTileHistogram[quantized_log_luma], 1);
	}

	GroupMemoryBarrierWithGroupSync();

	Histogram.InterlockedAdd(groupThreadID * 4, GroupTileHistogram[groupThreadID]);
}