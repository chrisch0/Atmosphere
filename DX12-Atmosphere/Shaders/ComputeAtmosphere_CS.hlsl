#include "AtmosphereCommon.hlsli"

Texture2D<float4> Transmittance : register(t0);
Texture3D<float4> Scattering : register(t1);
Texture2D<float4> Irradiance_Texture : register(t2);
Texture3D<float4> SingleMieScattering : register(t3);

cbuffer RenderCB : register(b0)
{
	float3 CameraPosition;
	float Exposure;
	float4 Resolution;
	float4x4 InvProj;
	float4x4 InvView;
	float3 WhitePoint;
	float EarthRadius;
	float3 SunDirection;
	float GroundAlbedo;
	float2 SunSize;
}
#define RADIANCE_API_ENABLED
#include "ComputeSkyCommon.hlsli"

// GetSunVisibility use shadowmap instead

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

	// if view ray not intersect ground, distance_to_ground is NaN
	float3 earth_center = float3(0.0, -EarthRadius, 0.0);
	float3 p = CameraPosition - earth_center;
	float p_dot_v = dot(p, world_dir);
	float p_dot_p = dot(p, p);
	float ray_earth_center_squared_distance = p_dot_p - p_dot_v * p_dot_v;
	float distance_to_ground = -p_dot_v - sqrt(EarthRadius * EarthRadius - ray_earth_center_squared_distance);

	float ground_albedo = 0.0;
	float3 ground_radiance = 0.0;
	if (distance_to_ground > 0.0)
	{
		float3 ground_pos = CameraPosition + distance_to_ground * world_dir;
		float3 normal = normalize(ground_pos - earth_center);

		float3 sky_irradiance;
		float3 sun_irradiance = GetSunAndSkyIrradiance(ground_pos - earth_center, normal, SunDirection, sky_irradiance);
		// TODO: sun_irradiance * sun_visibility
		ground_radiance = GroundAlbedo * (1.0 / PI) * (sun_irradiance + sky_irradiance);
		float3 transmittance;
		float3 in_scatter = GetSkyRadianceToPoint(CameraPosition - earth_center,
			ground_pos - earth_center, 0.0, SunDirection, transmittance);
		ground_radiance = ground_radiance * transmittance + in_scatter;
		ground_albedo = 1.0;
	}

	float3 transmittance;
	float3 radiance = GetSkyRadiance(
		CameraPosition - earth_center, world_dir, 0.0, SunDirection,
		transmittance);

	if (dot(world_dir, SunDirection) > SunSize.y)
	{
		radiance = radiance + transmittance * GetSolarRadiance();
	}
	radiance = lerp(radiance, ground_radiance, ground_albedo);
	float3 finalColor =
		pow(float3(1.0, 1.0, 1.0) - exp(-radiance * Exposure), 1.0 / 2.2);

}