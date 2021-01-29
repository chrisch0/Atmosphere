#ifdef RADIANCE_API_ENABLED
RadianceSpectrum GetSolarRadiance()
{
	return Atmosphere.solar_irradiance / (PI * Atmosphere.sun_angular_radius * Atmosphere.sun_angular_radius);
}

RadianceSpectrum GetSkyRadiance(
	Position camera, Direction view_ray, Length shadow_length,
	Direction sun_direction, out DimensionlessSpectrum transmittance
)
{
	return GetSkyRadiance(Atmosphere, Transmittance, Scattering, SingleMieScattering,
		camera, view_ray, shadow_length, sun_direction, transmittance);
}

RadianceSpectrum GetSkyRadianceToPoint(
	Position camera, Position pos, Length shadow_length,
	Direction sun_direction, out DimensionlessSpectrum transmittance
)
{
	return GetSkyRadianceToPoint(Atmosphere, Transmittance, Scattering, SingleMieScattering,
		camera, pos, shadow_length, sun_direction, transmittance);
}

RadianceSpectrum GetSunAndSkyIrradiance(
	Position p, Direction normal, Direction sun_direction,
	out IrradianceSpectrum sky_irradiance
)
{
	return GetSunAndSkyIrradiance(Atmosphere, Transmittance, Irradiance_Texture, p, normal, sun_direction, sky_irradiance);
}
#endif

Luminance3 GetSolarLuminance()
{
	return Atmosphere.solar_irradiance / (PI * Atmosphere.sun_angular_radius * Atmosphere.sun_angular_radius) * SUN_SPECTRAL_RADIANCE_TO_LUMINANCE;
}

Luminance3 GetSkyLuminance(
	Position camera, Direction view_ray, Length shadow_length,
	Direction sun_direction, out DimensionlessSpectrum transmittance
)
{
	return GetSkyRadiance(Atmosphere, Transmittance, Scattering, SingleMieScattering,
		camera, view_ray, shadow_length, sun_direction, transmittance) * SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
}

Luminance3 GetSkyLuminanceToPoint(
	Position camera, Position pos, Length shadow_length,
	Direction sun_direction, out DimensionlessSpectrum transmittance
)
{
	return GetSkyRadianceToPoint(Atmosphere, Transmittance, Scattering, SingleMieScattering,
		camera, pos, shadow_length, sun_direction, transmittance) * SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
}

Illuminance3 GetSunAndSkyIlluminance(
	Position p, Direction normal, Direction sun_direction,
	out IrradianceSpectrum sky_irradiance
)
{
	IrradianceSpectrum sun_irradiance = GetSunAndSkyIrradiance(Atmosphere, Transmittance, Irradiance_Texture, p, normal, sun_direction, sky_irradiance);
	sky_irradiance *= SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
	return sun_irradiance * SUN_SPECTRAL_RADIANCE_TO_LUMINANCE;
}

#ifdef USE_LUMINANCE
#define GetSolarRadiance GetSolarLuminance
#define GetSkyRadiance GetSkyLuminance
#define GetSkyRadianceToPoint GetSkyLuminanceToPoint
#define GetSunAndSkyIrradiance GetSunAndSkyIlluminance
#endif

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