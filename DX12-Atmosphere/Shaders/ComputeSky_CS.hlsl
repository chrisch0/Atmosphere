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
	float4x4 PrevViewProj;

	float3 CameraPosition;
	float Time;

	float3 LightDir;
	uint FrameIndex;

	float4 Resolution;

	float3 GroundAlbedo;
	float Exposure;

	float3 WhitePoint;
	float SunSize;
}

#define RADIANCE_API_ENABLED
#define EarthCenter float3(CameraPosition.x, -Atmosphere.bottom_radius, CameraPosition.z)

#include "AtmosphereCommon.hlsli"
#include "ComputeSkyCommon.hlsli"
#include "RaymarchPrimitives.hlsli"

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

float3 RenderPrimitives(in float3 ro, in float3 rd, out float cast_shadow, out float alpha)
{
	// background
	float3 col = float3(0.0, 0.0, 0.0);
	alpha = 0.0;
	// raycast scene
	float2 res = raycast(ro, rd);
	float t = res.x;
	float m = res.y;
	cast_shadow = 1.0;
	if (m > -0.5)
	{
		alpha = (m < 1.5) ? 0.0 : 1.0;
		float3 pos = ro + t * rd;
		float3 nor = calcNormal(pos);
		float3 ref = reflect(rd, nor);

		// material        
		col = 0.2 + 0.2*sin(m*2.0 + float3(0.0, 1.0, 2.0));
		float ks = 1.0;

		// lighting
		float occ = calcAO(pos, nor);

		float3 lin = float3(0.0, 0.0, 0.0);


		float3 sky_irradiance;
		float3 sun_irradiance = GetSunAndSkyIrradiance(pos - EarthCenter, LightDir, sky_irradiance);

		// sun
		{
			float3  lig = LightDir;
			float3  hal = normalize(lig - rd);
			float dif = clamp(dot(nor, lig), 0.0, 1.0);
			//if( dif>0.0001 )
			cast_shadow *= calcSoftshadow(pos, lig, 0.02, 2.5);
			dif *= cast_shadow;
			float spe = pow(clamp(dot(nor, hal), 0.0, 1.0), 16.0);
			spe *= dif;
			spe *= 0.04 + 0.96*pow(clamp(1.0 - dot(hal, lig), 0.0, 1.0), 5.0);
			lin += col * 2.20*dif*sun_irradiance;
			lin += 5.00*spe*sun_irradiance*ks;
		}
		// sky
		{
			float dif = sqrt(clamp(0.5 + 0.5*nor.y, 0.0, 1.0));
			dif *= occ;
			float spe = smoothstep(-0.2, 0.2, ref.y);
			spe *= dif;
			spe *= 0.04 + 0.96*pow(clamp(1.0 + dot(nor, rd), 0.0, 1.0), 5.0);
			//if( spe>0.001 )
			spe *= calcSoftshadow(pos, ref, 0.02, 2.5);
			lin += col * 0.60*dif*sky_irradiance;
			lin += 2.00*spe*sky_irradiance*ks;
		}
		//// back
		//{
		//	float dif = clamp(dot(nor, normalize(float3(0.5, 0.0, 0.6))), 0.0, 1.0)*clamp(1.0 - pos.y, 0.0, 1.0);
		//	dif *= occ;
		//	lin += col * 0.55*dif*float3(0.25, 0.25, 0.25);
		//}
		//// sss
		//{
		//	float dif = pow(clamp(1.0 + dot(nor, rd), 0.0, 1.0), 2.0);
		//	dif *= occ;
		//	lin += col * 0.25*dif*float3(1.00, 1.00, 1.00);
		//}
		float3 transmittance;
		float3 in_scatter = GetSkyRadianceToPoint(ro - EarthCenter, pos - EarthCenter, 0, LightDir, transmittance);
		lin = lin * transmittance + in_scatter;

		col = lin;
		//col = lerp(col, float3(0.7, 0.7, 0.9), 1.0 - exp(-0.0001*t*t*t));
	}

	//return float3(clamp(col, 0.0, 1.0));
	return col;
}

float3 Sun0(float centerToEdge)
{
	float3 u = float3(1.0, 1.0, 1.0); // some models have u !=1
	float3 a = float3(0.397, 0.503, 0.652); // coefficient for RGB wavelength (680 ,550 ,440)

	centerToEdge = 1.0 - centerToEdge;
	float mu = sqrt(1.0 - centerToEdge * centerToEdge);

	float3 factor = 1.0 - u * (1.0 - pow(float3(mu, mu, mu), a));
	return factor;
}

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float2 pixel_coord = float2(globalID.xy) + 0.5f;
	float3 world_dir = ComputeWorldViewDir(pixel_coord);

	float3 camera = CameraPosition;
	camera.y *= 0.001f;
	float3 p = CameraPosition - EarthCenter;

	float primitive_alpha = 0.0;
	float cast_shadow = 1.0;
	float3 primitive_radiance = RenderPrimitives(CameraPosition, world_dir, cast_shadow, primitive_alpha);

	float p_dot_v = dot(p, world_dir);
	float p_dot_p = dot(p, p);
	float ray_earth_center_squared_distance = p_dot_p - p_dot_v * p_dot_v;
	float distance_to_intersection = -p_dot_v - sqrt(EarthCenter.y * EarthCenter.y - ray_earth_center_squared_distance);

	float ground_alpha = 0.0;
	float3 ground_radiance = 0.0;
	if (distance_to_intersection > 0.0)
	{
		float3 intersection_point = CameraPosition + distance_to_intersection * world_dir;
		float3 ground_normal = normalize(intersection_point - EarthCenter);
		float3 sky_irradiance;
		float3 sun_irradiance = GetSunAndSkyIrradiance(intersection_point - EarthCenter, LightDir, sky_irradiance);
		ground_radiance = GroundAlbedo * (1.0 / PI) * (sun_irradiance * max(dot(ground_normal, LightDir), 0.0) * cast_shadow + sky_irradiance);
		float3 transmittance;
		float3 in_scatter = GetSkyRadianceToPoint(CameraPosition - EarthCenter, intersection_point - EarthCenter, 0, LightDir, transmittance);
		ground_radiance = ground_radiance * transmittance + in_scatter;
		ground_alpha = 1.0;
	}

	float3 transmittance;
	float3 radiance = GetSkyRadiance(CameraPosition - EarthCenter, world_dir, 0, LightDir, transmittance);
	float costh = dot(world_dir, LightDir);
	if (costh > SunSize)
	{
		float d = 1.0 - sqrt(1.0 - costh * costh);
		radiance = radiance * (1 - d * d) + transmittance * GetSolarRadiance() * Sun0(costh) * d * d;
	}

	radiance = lerp(radiance, ground_radiance, ground_alpha);
	radiance = lerp(radiance, primitive_radiance, primitive_alpha);
	float4 color;
	color.rgb = pow(1.0 - exp(-radiance / WhitePoint * Exposure), 1.0 / 2.2);
	color.a = 1.0;
	SceneBuffer[globalID.xy] = color;
}