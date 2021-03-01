Texture2D<float4> Transmittance : register(t0);
Texture3D<float4> Scattering : register(t1);
Texture2D<float4> Irradiance_Texture : register(t2);
Texture3D<float4> SingleMieScattering : register(t3);
//Texture2D<float> DepthBuffer : register(t4);

RWTexture2D<float4> SceneBuffer : register(u0);

cbuffer PassCB : register(b0)
{
	float4x4 InvView;
	float4x4 InvProj;

	float3 CameraPosition;
	float Exposure;

	float3 LightDir;
	float SunSize;

	float4 Resolution;

	float3 WhitePoint;
	float pad2;

	float3 EarthCenter;
	float pad3;

	float3 GroundAlbedo;
}

#define RADIANCE_API_ENABLED

#include "AtmosphereCommon.hlsli"
#include "ComputeSkyCommon.hlsli"

float3 ScreenToClip(float2 screenPos)
{
	float2 xy = screenPos * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
	return float3(xy, 1.0f);
}

float3 ComputeWorldViewDir(float2 pixelCoord)
{
	float3 clip_coord = ScreenToClip(pixelCoord * Resolution.zw);
	float4 view_coord = mul(InvProj, float4(clip_coord, 1.0));
	//view_coord = float4(view_coord.xy, -1.0, 0.0);
	view_coord /= view_coord.w;
	float3 world_dir = mul(InvView, float4(view_coord.xyz, 0.0)).xyz;
	world_dir = normalize(world_dir);
	return world_dir;
}

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float2 pixel_coord = float2(globalID.xy) + 0.5f;
	float3 world_dir = ComputeWorldViewDir(pixel_coord);

	float3 p = CameraPosition - EarthCenter;
	float p_dot_v = dot(p, world_dir);
	float p_dot_p = dot(p, p);
	float ray_earth_center_squared_distance = p_dot_p - p_dot_v * p_dot_v;
	float distance_to_intersection = -p_dot_v - sqrt(EarthCenter.y * EarthCenter.y - ray_earth_center_squared_distance);

	float ground_alpha = 0.0;
	float3 ground_radiance;
	if (distance_to_intersection > 0.0)
	{
		float3 intersection_point = CameraPosition + distance_to_intersection * world_dir;
		float3 normal = normalize(intersection_point - EarthCenter);
		float3 sky_irradiance;
		float3 sun_irradiance = GetSunAndSkyIrradiance(intersection_point - EarthCenter, normal, LightDir, sky_irradiance);
		ground_radiance = GroundAlbedo * (1.0 / PI) * (sun_irradiance + sky_irradiance);
		float3 transmittance;
		float3 in_scatter = GetSkyRadianceToPoint(CameraPosition - EarthCenter, intersection_point - EarthCenter, 0, LightDir, transmittance);
		ground_radiance = ground_radiance * transmittance + in_scatter;
		ground_alpha = 1.0;
	}

	float3 transmittance;
	float3 radiance = GetSkyRadiance(CameraPosition - EarthCenter, world_dir, 0, LightDir, transmittance);
	if (dot(world_dir, LightDir) > SunSize)
	{
		radiance += transmittance * GetSolarRadiance();
	}
	radiance = lerp(radiance, ground_radiance, ground_alpha);
	float4 color;
	color.rgb = pow(1.0 - exp(-radiance / WhitePoint * Exposure), 1.0 / 2.2);
	color.a = 1.0;
	SceneBuffer[globalID.xy] = color;
}