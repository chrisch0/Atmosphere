//Texture3D<float4> CloudShapeTexture : register(t0);
//Texture3D<float4> ErosionTexture : register(t1);
//Texture2D<float4> WeatherTexture : register(t2);
//Texture2D<float4> PreCloudColor : register(t3);
//Texture2D<float4> CurlNoise : register(t4);
//
//RWTexture2D<float4> CloudColor : register(u0);
//
//SamplerState LinearRepeatSampler : register(s0);

#define RADIANCE_API_ENABLED
#include "VolumetricCloudCommon.hlsli"

static const float2 HaltonSequence[] =
{
	float2(0.5, 0.333333),
	float2(0.25, 0.666667),
	float2(0.75, 0.111111),
	float2(0.125, 0.444444),
	float2(0.625, 0.777778),
	float2(0.375, 0.222222),
	float2(0.875, 0.555556),
	float2(0.0625, 0.888889),
	float2(0.5625, 0.037037),
	float2(0.3125, 0.37037),
	float2(0.8125, 0.703704),
	float2(0.1875, 0.148148),
	float2(0.6875, 0.481481),
	float2(0.4375, 0.814815),
	float2(0.9375, 0.259259),
	float2(0.03125, 0.592593)
};

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID, uint3 groupThreadID : SV_GroupThreadID )
{
	float4 frag_color, bloom, alphaness, cloud_distance;
	frag_color = float4(0.0, 0.0, 0.0, 1.0);
	//float2 pixel_coord = float2(globalID.xy) + 0.5f;
	uint frame_index = FrameIndex % 16;
	float2 pixel_coord = globalID.xy * float2(4, 4) + float2(frame_index % 4, frame_index / 4);
	float3 world_dir = ComputeWorldViewDir(pixel_coord);
	float2 uv = WorldViewDirToUV(world_dir, PrevViewProj);

	uint2 quarter_group_id = groupThreadID.xy % uint2(4, 4);
	uint quarter_group_index = quarter_group_id.y * 4 + quarter_group_id.x;

	float3 start_pos, end_pos;
	float4 v = 0.0f;

	float4 bg = float4(0.4851, 0.5986, 0.7534, 1.0);
	float3 fogRay;
	// assert camera near ground
	RaySphereIntersection(CameraPosition, world_dir, INNER_RADIUS, start_pos);
	RaySphereIntersection(CameraPosition, world_dir, OUTER_RADIUS, end_pos);
	fogRay = start_pos;

	// sun
	float sun = dot(LightDir, world_dir);
	if (sun > 0.1)  // cos(0.5 * angular_diameter)
	{
		float3 s = 0.8*float3(1.0, 0.4, 0.2)*pow(sun, 256.0);
		frag_color.rgb += float3(1.0, 1.0, 1.0);
	}

	float3 ground_pos;
	//bool ground = RaySphereIntersection(CameraPosition, world_dir, EarthRadius * 10.0, ground_pos);
	bool ground = RayIntersectGround(CameraPosition, world_dir, EarthRadius * 10.0, ground_pos);
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

	frag_color += bg;
	alphaness = float4(cloud_alphaness, 0.0, 0.0, 1.0);

	

	frag_color.a = alphaness.r;

	CloudColor[globalID.xy] = frag_color;
}