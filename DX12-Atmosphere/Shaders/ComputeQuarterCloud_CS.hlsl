Texture3D<float4> CloudShapeTexture : register(t0);
Texture3D<float4> ErosionTexture : register(t1);
Texture2D<float4> WeatherTexture : register(t2);

RWTexture2D<float4> CloudColor : register(u0);
RWTexture2D<float4> PreCloudColor : register(u1);

SamplerState LinearRepeatSampler : register(s0);

#include "VolumetricCloudCommon.hlsli"


[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID, uint3 groupThreadID : SV_GroupThreadID )
{
	float4 frag_color, bloom, alphaness, cloud_distance;
	float2 pixel_coord = float2(globalID.xy) + 0.5f;
	float3 world_dir = ComputeWorldViewDir(pixel_coord);
	float2 uv = WorldViewDirToUV(world_dir, PrevViewProj);

	uint frame_index = FrameIndex % 16;
	uint2 quarter_group_id = groupThreadID.xy % uint2(4, 4);
	uint quarter_group_index = quarter_group_id.y * 4 + quarter_group_id.x;
	
	// do raymarch
	if (frame_index == quarter_group_index || (any(uv < 0.0) || any(uv > 1.0)))
	{
		float3 start_pos, end_pos;
		float4 v = 0.0f;

		float4 bg = float4(0.4851, 0.5986, 0.7534, 1.0);
		float3 fogRay;
		// assert camera near ground
		RaySphereIntersection(CameraPosition, world_dir, INNER_RADIUS, start_pos);
		RaySphereIntersection(CameraPosition, world_dir, OUTER_RADIUS, end_pos);
		fogRay = start_pos;

		float3 ground_pos;
		bool ground = RaySphereIntersection(CameraPosition, world_dir, EarthRadius, ground_pos);
		if (ground)
		{
			CloudColor[globalID.xy] = float4(0.0, 0.2, 0.87, 1.0);
			return;
		}

		v = RaymarchCloud(globalID.xy, start_pos, end_pos, bg.rgb, cloud_distance);

		float cloud_alphaness = Threshold(v.a, 0.2);
		v.rgb = v.rgb * 1.8 - 0.1;

		bg.rgb = bg.rgb * (1.0 - v.a) + v.rgb;
		bg.a = 1.0;

		frag_color = bg;
		alphaness = float4(cloud_alphaness, 0.0, 0.0, 1.0);

		frag_color.a = alphaness.r;

		CloudColor[globalID.xy] = frag_color;
	}
	else
	{
		uint2 tex_coord = uint2(uv * Resolution.xy);
		CloudColor[globalID.xy] = PreCloudColor[tex_coord];
	}
}