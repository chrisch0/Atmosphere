

#define RADIANCE_API_ENABLED
#include "VolumetricCloudCommon.hlsli"

float HPDot(float3 FloorV0, float3 FractV0, float3 FloorV1, float3 FractV1, out float fractR)
{
	float inter0 = dot(FloorV0, FractV1);
	float inter1 = dot(FractV0, FloorV1);
	fractR = frac(inter0) + frac(inter1) + dot(FractV0, FractV1);
	float floorR = floor(inter0) + floor(inter1) + dot(FloorV0, FloorV1) + floor(fractR);
	fractR = frac(fractR);
	return floorR;
}

float HPDot(float3 floor_v0, float fract_v0, float3 v1, out float fractR)
{
	fractR = dot(fract_v0, v1);
	float floorR = dot(floor_v0, v1);
	fractR = frac(floorR) + frac(floorR);
	floorR = floor(floorR) + floor(fractR);
	fractR = frac(fractR);
	return floorR;
}

float HP2(float fl, float fr, out float frR)
{
	float flR = fl * fl;
	frR = fr * fr;

}

[numthreads(8, 8, 1)]
void main(uint3 globalID : SV_DispatchThreadID, uint3 groupThreadID : SV_GroupThreadID)
{
	float4 frag_color, bloom, alphaness, cloud_distance;
	float2 pixel_coord = float2(globalID.xy) + 0.5f;
	float3 world_dir = ComputeWorldViewDir(pixel_coord);
	float2 uv = WorldViewDirToUV(world_dir, PrevViewProj);

	float3 EarthCenter = float3(CameraPosition.x, -EarthRadius, CameraPosition.z);
	float3 camera = CameraPosition;
	camera.y *= 0.001f;
	float3 p = camera - EarthCenter;

	float p_dot_v = dot(p, world_dir);
	float p_dot_p = dot(p, p);
	float ray_earth_center_squared_distance = p_dot_p - p_dot_v * p_dot_v;
	float distance_to_intersection = -p_dot_v - sqrt(EarthCenter.y * EarthCenter.y - ray_earth_center_squared_distance);
	
	/*float3 floor_p = floor(p);
	float3 fract_p = frac(p);
	float fract_p_dot_v;
	float floor_p_dot_v = HPDot(floor_p, fract_p, world_dir, fract_p_dot_v);
	float fract_p_dot_p;
	float floor_p_dot_p = HPDot(floor_p, fract_p, floor_p, fract_p, fract_p_dot_p);
	float fl_r = floor_p_dot_p - floor_p_dot_v * floor_p_dot_v;
	float fr_r = fract_p_dot_p - fract_p_dot_v * fract_p_dot_v;
	float distance_to_intersection = -(floor_p_dot_v + fract_p_dot_v) - sqrt(EarthCenter.y * EarthCenter.y - fl_r + fr_r);*/

	float ground_alpha = 0.0;
	float3 ground_radiance;
	if (distance_to_intersection > 0.0)
	{
		float3 intersection_point = camera + distance_to_intersection * world_dir;
		float3 normal = normalize(intersection_point - EarthCenter);
		float3 sky_irradiance;
		float3 sun_irradiance = GetSunAndSkyIrradiance(intersection_point - EarthCenter, normal, LightDir, sky_irradiance);
		ground_radiance = GroundAlbedo * (1.0 / PI) * (sun_irradiance + sky_irradiance);
		float3 transmittance;
		float3 in_scatter = GetSkyRadianceToPoint(camera - EarthCenter, intersection_point - EarthCenter, 0, LightDir, transmittance);
		ground_radiance = ground_radiance * transmittance + in_scatter;
		ground_alpha = 1.0;
	}
	float3 transmittance;
	float3 radiance = GetSkyRadiance(camera - EarthCenter, world_dir, 0, LightDir, transmittance);
	if (dot(world_dir, LightDir) > SunSize)
	{
		radiance += transmittance * GetSolarRadiance();
	}
	radiance = lerp(radiance, ground_radiance, ground_alpha);
	frag_color.rgb = pow(1.0 - exp(-radiance / WhitePoint * Exposure), 1.0 / 2.2);
	frag_color.a = 1.0;

	if (distance_to_intersection > 0.0)
	{
		CloudColor[globalID.xy] = frag_color;
		return;
	}

	//float3 ground_pos;
	////bool ground = RaySphereIntersection(CameraPosition, world_dir, EarthRadius * 10.0, ground_pos);
	//bool ground = RayIntersectGround(CameraPosition, world_dir, EarthRadius * 0.01, ground_pos);
	//if (ground)
	//{
	//	//CloudColor[globalID.xy] = float4(0.0, 0.2, 0.87, 1.0);
	//	return;
	//}

	uint frame_index = FrameIndex % 16;
	uint2 quarter_group_id = groupThreadID.xy % uint2(4, 4);
	uint quarter_group_index = quarter_group_id.y * 4 + quarter_group_id.x;

	// do raymarch
	if (frame_index == quarter_group_index || (any(uv < 0.0) || any(uv > 1.0)))
	{
		float3 start_pos, end_pos;
		float4 v = 0.0f;

		float4 bg = frag_color;
		float3 fogRay;
		// assert camera near ground
		RaySphereIntersection(CameraPosition, world_dir, INNER_RADIUS, start_pos);
		RaySphereIntersection(CameraPosition, world_dir, OUTER_RADIUS, end_pos);
		
		fogRay = start_pos;
		cloud_distance.xyz = CameraPosition;

		v = RaymarchCloud(globalID.xy, start_pos, end_pos, bg.rgb, cloud_distance);
		//float3 cloud_bottom_pos = (cloud_distance - start_pos) * 0.001 + 
		transmittance = GetTransmittanceToPoint(CameraPosition * 0.001 - float3(0.0, -EarthRadius, 0.0), cloud_distance.xyz * 0.00001 - float3(0.0, -EarthRadius, 0.0), LightDir);

		float cloud_alphaness = Threshold(v.a, 0.2);
		v.rgb = (v.rgb * 1.8 - 0.1) * transmittance;
		//v.rgb = pow(v.rgb, pad_CP0);

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
		//uv = (float2(tex_coord) + HaltonSequence[( quarter_group_index) % 16]) * Resolution.zw;
		//CloudColor[globalID.xy] = PreCloudColor[tex_coord];
		CloudColor[globalID.xy] = PreCloudColor.SampleLevel(LinearRepeatSampler, uv, 0);
		//CloudColor[globalID.xy] = noise(float3(globalID));
	}
}