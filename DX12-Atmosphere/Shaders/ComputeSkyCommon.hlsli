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
	Position p, /*Direction normal,*/ Direction sun_direction,
	out IrradianceSpectrum sky_irradiance
)
{
	return GetSunAndSkyIrradiance(Atmosphere, Transmittance, Irradiance_Texture, p, /*normal,*/ sun_direction, sky_irradiance);
}

// p ref to earth center
RadianceSpectrum GetSunAndSkyIrradianceAtPoint(
	Position p, Direction sun_direction,
	out IrradianceSpectrum sky_irradiance
)
{
	Length r = length(p);
	Number mu_s = dot(p, sun_direction) / r;
	sky_irradiance = GetIrradiance(Atmosphere, Irradiance_Texture, r, mu_s);
	return Atmosphere.solar_irradiance * GetTransmittanceToSun(Atmosphere, Transmittance, r, mu_s);
}

DimensionlessSpectrum GetTransmittanceToPoint(
	Position camera, Position pos, Direction sun_direction
)
{
	Direction view_ray = normalize(pos - camera);
	Length r = length(camera);
	Length rmu = dot(camera, view_ray);
	// assert(camera is in atmosphere)
	Number mu = rmu / r;
	Number mu_s = dot(camera, sun_direction) / r;
	Number nu = dot(view_ray, sun_direction);
	Length d = length(pos - camera);
	bool ray_r_mu_intersects_ground = RayIntersectsGround(Atmosphere, r, mu);
	return GetTransmittance(Atmosphere, Transmittance, r, mu, d, ray_r_mu_intersects_ground);

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
	IrradianceSpectrum sun_irradiance = GetSunAndSkyIrradiance(Atmosphere, Transmittance, Irradiance_Texture, p, /*normal,*/ sun_direction, sky_irradiance);
	sky_irradiance *= SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
	return sun_irradiance * SUN_SPECTRAL_RADIANCE_TO_LUMINANCE;
}

#ifdef USE_LUMINANCE
#define GetSolarRadiance GetSolarLuminance
#define GetSkyRadiance GetSkyLuminance
#define GetSkyRadianceToPoint GetSkyLuminanceToPoint
#define GetSunAndSkyIrradiance GetSunAndSkyIlluminance
#endif