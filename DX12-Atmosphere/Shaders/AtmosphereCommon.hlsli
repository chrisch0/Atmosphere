#define Length float
#define Wavelength float
#define Angle float
#define SolidAngle float
#define Power float
#define LuminousPower float
#define Number float
#define InverseLength float
#define Area float
#define Volume float
#define NumberDensity float
#define Irradiance float
#define Radiance float
#define SpectralPower float
#define SpectralIrradiance float
#define SpectralRadiance float
#define SpectralRadianceDensity float
#define ScatteringCoefficient float
#define InverseSolidAngle float
#define LuminousIntensity float
#define Luminance float
#define Illuminance float
#define AbstractSpectrum float3
#define DimensionlessSpectrum float3
#define PowerSpectrum float3
#define IrradianceSpectrum float3
#define RadianceSpectrum float3
#define RadianceDensitySpectrum float3
#define ScatteringSpectrum float3
#define Position float3
#define Direction float3
#define Luminance3 float3
#define Illuminance3 float3
#define TransmittanceTexture Texture2D<float4>
#define AbstractScatteringTexture Texture3D<float4>
#define ReducedScatteringTexture Texture3D<float4>
#define ScatteringTexture Texture3D<float4>
#define ScatteringDensityTexture Texture3D<float4>
#define IrradianceTexture Texture2D<float4>

static const Length m = 1.0;
static const Wavelength nm = 1.0;
static const Angle rad = 1.0;
static const SolidAngle sr = 1.0;
static const Power watt = 1.0;
static const LuminousPower lm = 1.0;
static const float PI = 3.14159265358979323846;
static const Length km = 1000.0 * m;
static const Area m2 = m * m;
static const Volume m3 = m * m * m;
static const Angle pi = PI * rad;
static const Angle deg = pi / 180.0;
static const Irradiance watt_per_square_meter = watt / m2;
static const Radiance watt_per_square_meter_per_sr = watt / (m2 * sr);
static const SpectralIrradiance watt_per_square_meter_per_nm = watt / (m2 * nm);
static const SpectralRadiance watt_per_square_meter_per_sr_per_nm =
watt / (m2 * sr * nm);
static const SpectralRadianceDensity watt_per_cubic_meter_per_sr_per_nm =
watt / (m3 * sr * nm);
static const LuminousIntensity cd = lm / sr;
static const LuminousIntensity kcd = 1000.0 * cd;
static const Luminance cd_per_square_meter = cd / m2;
static const Luminance kcd_per_square_meter = kcd / m2;

struct DensityParameter {
	Length width;
	Number exp_term;
	InverseLength exp_scale;
	InverseLength linear_term;
	Number constant_term;
	float3 pad;
};

struct DensityProfile {
	DensityParameter params[2];
};

struct AtmosphereParameters {
	IrradianceSpectrum solar_irradiance;
	Angle sun_angular_radius;
	ScatteringSpectrum absorption_extinction;
	Length bottom_radius;
	DimensionlessSpectrum ground_albedo;
	Length top_radius;
	ScatteringSpectrum rayleigh_scattering;
	Number mie_phase_function_g;
	ScatteringSpectrum mie_scattering;
	Number mu_s_min;
	ScatteringSpectrum mie_extinction;
	float pad;
	DensityProfile rayleigh_density;
	DensityProfile mie_density;
	DensityProfile absorption_density;
};

static const int TRANSMITTANCE_TEXTURE_WIDTH = 256;
static const int TRANSMITTANCE_TEXTURE_HEIGHT = 64;
static const int SCATTERING_TEXTURE_R_SIZE = 32;
static const int SCATTERING_TEXTURE_MU_SIZE = 128;
static const int SCATTERING_TEXTURE_MU_S_SIZE = 32;
static const int SCATTERING_TEXTURE_NU_SIZE = 8;
static const int IRRADIANCE_TEXTURE_WIDTH = 64;
static const int IRRADIANCE_TEXTURE_HEIGHT = 16;

cbuffer Parameter : register(b1)
{
	AtmosphereParameters Atmosphere;
	float3 SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
	float pad0;
	float3 SUN_SPECTRAL_RADIANCE_TO_LUMINANCE;
	float pad1;
};

SamplerState LinearClampSampler : register(s0);

Number ClampCosine(Number mu) {
	return clamp(mu, Number(-1.0), Number(1.0));
}

Length ClampDistance(Length d) {
	return max(d, 0.0 * m);
}

Length ClampRadius(const in AtmosphereParameters atmosphere, Length r) {
	return clamp(r, atmosphere.bottom_radius, atmosphere.top_radius);
}

Length SafeSqrt(Area a) {
	return sqrt(max(a, 0.0 * m2));
}

// Calculate the distance from the pos to the top atmosphere boundary
Length DistanceToTopAtmosphereBoundary(
	const in AtmosphereParameters atmosphere,
	Length r,
	Number mu
)
{
	//assert(r <= atmosphere.top_radius);
	//assert(mu >= -1.0 && mu <= 1.0);
	Area discriminant = r * r * (mu * mu - 1.0) + atmosphere.top_radius * atmosphere.top_radius;
	return ClampDistance(-r * mu + SafeSqrt(discriminant));
}

// Calculate the distance from the pos to the bottom atmosphere boundary(ground)
Length DistanceToBottomAtmosphereBoundary(
	const in AtmosphereParameters atmosphere,
	Length r,
	Number mu
)
{
	//assert(r >= atmosphere.bottom_radius);
	//assert(mu >= -1.0 && mu <= 1.0);
	Area discriminant = r * r * (mu * mu - 1.0) + atmosphere.bottom_radius * atmosphere.bottom_radius;
	return ClampDistance(-r * mu - SafeSqrt(discriminant));
}

// Check if the ray intersects with ground
bool RayIntersectsGround(
	const in AtmosphereParameters atmosphere,
	Length r,
	Number mu
)
{
	//assert(r >= atmosphere.bottom_radius);
	//assert(mu >= -1.0 && mu <= 1.0);
	return mu < 0.0 && r * r * (mu * mu - 1.0) + atmosphere.bottom_radius * atmosphere.bottom_radius >= 0.0 * m2;
}

Number CalculateDensity(
	const in DensityParameter param,
	Length altitude
)
{
	Number density = param.exp_term * exp(param.exp_scale * altitude) +
		param.linear_term * altitude + param.constant_term;
	return clamp(density, Number(0.0), Number(1.0));
}

Number GetDensity(
	const in DensityProfile profile,
	Length altitude
)
{
	return altitude < profile.params[0].width ?
		CalculateDensity(profile.params[0], altitude) :
		CalculateDensity(profile.params[1], altitude);
}

// **** Precompute the transmittance to the top atmosphere boundary **** //

// use r, mu, scattering density profile compute optical length from pos p to top atmosphere boundary
Length ComputeOpticalLengthToTopAtmosphereBoundary(
	const in AtmosphereParameters atmosphere,
	const in DensityProfile profile,
	Length r,
	Number mu
)
{
	//assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);
	//assert(mu >= -1.0 && mu <= 1.0);
	const int SAMPLE_COUNT = 500;
	Length dx = DistanceToTopAtmosphereBoundary(atmosphere, r, mu) / Number(SAMPLE_COUNT);
	Length result = 0.0 * m;
	for (int i = 0; i <= SAMPLE_COUNT; ++i)
	{
		Length d_i = Number(i) * dx;
		Length r_i = sqrt(d_i * d_i + 2.0 * r * mu * d_i + r * r);
		Number y_i = GetDensity(profile, r_i - atmosphere.bottom_radius);
		Number weight_i = i == 0 || i == SAMPLE_COUNT ? 0.5 : 1.0;
		result += y_i * weight_i * dx;
	}
	return result;
}

// use r, mu compute transmittance from pos p to top atmosphere boundary
DimensionlessSpectrum ComputeTransmittanceToTopAtmosphereBoundary(
	const in AtmosphereParameters atmosphere,
	Length r,
	Number mu
)
{
	//assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);
	//assert(mu >= -1.0 && mu <= 1.0);
	return exp(-(
		atmosphere.rayleigh_scattering * ComputeOpticalLengthToTopAtmosphereBoundary(atmosphere, atmosphere.rayleigh_density, r, mu) +
		atmosphere.mie_extinction * ComputeOpticalLengthToTopAtmosphereBoundary(atmosphere, atmosphere.mie_density, r, mu) +
		atmosphere.absorption_extinction * ComputeOpticalLengthToTopAtmosphereBoundary(atmosphere, atmosphere.absorption_density, r, mu)
		));
}

// map x[0, 1] -> u[0.5 / texture_size, 1.0 - 0.5 / texture_size]
Number GetTextureCoordFromUnitRange(
	Number x,
	int texture_size
)
{
	return 0.5 / Number(texture_size) + x * (1.0 - 1.0 / Number(texture_size));
}

// map u[0.5 / texture_size, 1.0 - 0.5 / texture_size] -> x[0, 1]
Number GetUnitRangeFromTextureCoord(
	Number u,
	int texture_size
)
{
	return (u - 0.5 / Number(texture_size)) / (1.0 - 1.0 / Number(texture_size));
}

// normalize (mu, r) and map to (u, v)
float2 GetTransmittanceTextureUVFromRMu(
	const in AtmosphereParameters atmosphere,
	Length r,
	Number mu
)
{
	//assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);
	//assert(mu >= -1.0 && mu <= 1.0);
	Length H = sqrt(atmosphere.top_radius * atmosphere.top_radius - atmosphere.bottom_radius * atmosphere.bottom_radius);
	Length rho = SafeSqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);
	Length d = DistanceToTopAtmosphereBoundary(atmosphere, r, mu);
	Length d_min = atmosphere.top_radius - r;
	Length d_max = rho + H;
	Number x_mu = (d - d_min) / (d_max - d_min);
	Number x_r = rho / H;
	return float2(GetTextureCoordFromUnitRange(x_mu, TRANSMITTANCE_TEXTURE_WIDTH),
		GetTextureCoordFromUnitRange(x_r, TRANSMITTANCE_TEXTURE_HEIGHT));
}

// map (u, v) -> (mu, r)
void GetRMuFromTransmittanceTextureUV(
	const in AtmosphereParameters atmosphere,
	const in float2 uv,
	out Length r,
	out Length mu
)
{
	//assert(uv.x >= 0.0 && uv.x <= 1.0);
	//assert(uv.y >= 0.0 && uv.y <= 1.0);
	Number x_mu = GetUnitRangeFromTextureCoord(uv.x, TRANSMITTANCE_TEXTURE_WIDTH);
	Number x_r = GetUnitRangeFromTextureCoord(uv.y, TRANSMITTANCE_TEXTURE_HEIGHT);
	Length H = sqrt(atmosphere.top_radius * atmosphere.top_radius - atmosphere.bottom_radius * atmosphere.bottom_radius);
	Length rho = H * x_r;
	r = sqrt(rho * rho + atmosphere.bottom_radius * atmosphere.bottom_radius);
	Length d_min = atmosphere.top_radius - r;
	Length d_max = rho + H;
	Length d = d_min + x_mu * (d_max - d_min);
	mu = d == 0.0 * m ? Number(1.0) : (H * H - rho * rho - d * d) / (2.0 * r * d);
	mu = ClampCosine(mu);
}

DimensionlessSpectrum ComputeTransmittanceToTopAtmosphereBoundaryTexture(
	const in AtmosphereParameters atmosphere,
	const in float2 frag_coord
)
{
	const float2 TRANSMITTANCE_TEXTURE_SIZE = float2(TRANSMITTANCE_TEXTURE_WIDTH, TRANSMITTANCE_TEXTURE_HEIGHT);
	Length r;
	Number mu;
	GetRMuFromTransmittanceTextureUV(atmosphere, frag_coord / TRANSMITTANCE_TEXTURE_SIZE, r, mu);
	return ComputeTransmittanceToTopAtmosphereBoundary(atmosphere, r, mu);
}

// ****** Transmittance lookup ****** //

// use r, mu get transmittance between p and top atmosphere boundary from texture
DimensionlessSpectrum GetTransmittanceToTopAtmosphereBoundary(
	const in AtmosphereParameters atmosphere,
	const in TransmittanceTexture transmittance_texture,
	Length r,
	Number mu
)
{
	// assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);
	float2 uv = GetTransmittanceTextureUVFromRMu(atmosphere, r, mu);
	return DimensionlessSpectrum(transmittance_texture.SampleLevel(LinearClampSampler, uv, 0.0).xyz);
}

// return the transmittance between pos p and q (p along ray mu at distance d reaches q)
DimensionlessSpectrum GetTransmittance(
	const in AtmosphereParameters atmosphere,
	const in TransmittanceTexture transmittance_texture,
	Length r,
	Number mu,
	Length d,
	bool ray_r_mu_intersects_ground
)
{
	// assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);
	// assert(mu >= -1.0 && mu <= 1.0);
	// assert(d >= 0.0 * m);
	Length r_d = ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));
	Number mu_d = ClampCosine((r * mu + d) / r_d);

	if (ray_r_mu_intersects_ground)
	{
		return min(
			GetTransmittanceToTopAtmosphereBoundary(atmosphere, transmittance_texture, r_d, -mu_d) / GetTransmittanceToTopAtmosphereBoundary(atmosphere, transmittance_texture, r, -mu),
			DimensionlessSpectrum(1.0, 1.0, 1.0)
		);
	}
	else
	{
		return min(
			GetTransmittanceToTopAtmosphereBoundary(atmosphere, transmittance_texture, r, mu) / GetTransmittanceToTopAtmosphereBoundary(atmosphere, transmittance_texture, r_d, mu_d),
			DimensionlessSpectrum(1.0, 1.0, 1.0)
		);
	}
}

// return transmittance between a pos in the atmosphere and the Sun.
// smoothstep(-alpha_s * sin theta_h, alpha_s * sin theta_h, cos theta_s - cos theta_h)
DimensionlessSpectrum GetTransmittanceToSun(
	const in AtmosphereParameters atmosphere,
	const in TransmittanceTexture transmittance_texture,
	Length r,
	Number mu_s
)
{
	Number sin_theta_h = atmosphere.bottom_radius / r;
	Number cos_theta_h = -sqrt(max(1.0 - sin_theta_h * sin_theta_h, 0.0));
	return GetTransmittanceToTopAtmosphereBoundary(atmosphere, transmittance_texture, r, mu_s) *
		smoothstep(-sin_theta_h * atmosphere.sun_angular_radius / rad,
			sin_theta_h * atmosphere.sun_angular_radius / rad,
			mu_s - cos_theta_h);
}

// ********************** Single scattering precompute ********************** //

// return rayleigh/mie_density at pos q * transmittance between q and sun * transmittance between pos p and q
void ComputeSingleScatteringIntegrand(
	const in AtmosphereParameters atmosphere,
	const in TransmittanceTexture transmittance_texture,
	Length r,
	Number mu,
	Number mu_s,
	Number nu,
	Length d,
	bool ray_r_mu_intersects_ground,
	out DimensionlessSpectrum rayleigh,
	out DimensionlessSpectrum mie
)
{
	Length r_d = ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));
	Number mu_s_d = ClampCosine((r * mu_s + d * nu) / r_d);
	DimensionlessSpectrum transmittance =
		GetTransmittance(atmosphere, transmittance_texture, r, mu, d, ray_r_mu_intersects_ground)
		* GetTransmittanceToSun(atmosphere, transmittance_texture, r_d, mu_s_d);
	rayleigh = transmittance * GetDensity(atmosphere.rayleigh_density, r_d - atmosphere.bottom_radius);
	mie = transmittance * GetDensity(atmosphere.mie_density, r_d - atmosphere.bottom_radius);
}

// calculate the distance to the nearest atmosphere boundary along ray_r_mu (top atmosphere boundary or ground)
Length DistanceToNearestAtmosphereBoundary(
	const in AtmosphereParameters atmosphere,
	Length r,
	Number mu,
	bool ray_r_mu_intersects_ground
)
{
	if (ray_r_mu_intersects_ground)
	{
		return DistanceToBottomAtmosphereBoundary(atmosphere, r, mu);
	}
	else
	{
		return DistanceToTopAtmosphereBoundary(atmosphere, r, mu);
	}
}

// return integral of the single scatter 
// from p to the atmosphere boundary(p intersects atmosphere boundary along ray_r_mu)
// sun_irradance * scattering_coefficient * \int_p^i {T_pq(t) * T_qsun(t) * scatter_density_q(t) * dt}
void ComputeSingleScattering(
	const in AtmosphereParameters atmosphere,
	const in TransmittanceTexture transmittance_texture,
	Length r,
	Number mu,
	Number mu_s,
	Number nu,
	bool ray_r_mu_intersects_ground,
	out IrradianceSpectrum rayleigh,
	out IrradianceSpectrum mie
)
{
	// assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);
	// assert(mu >= -1.0 && mu <= 1.0);
	// assert(mu_s >= -1.0 && mu_s <= 1.0);
	// assert(nu >= -1.0 && nu <= 1.0);

	// Number of intervals for the numerical integration
	const int SAMPLE_COUNT = 50;
	Length dx = DistanceToNearestAtmosphereBoundary(atmosphere, r, mu, ray_r_mu_intersects_ground) / Number(SAMPLE_COUNT);

	DimensionlessSpectrum rayleigh_sum = 0.0;
	DimensionlessSpectrum mie_sum = 0.0;
	for (int i = 0; i <= SAMPLE_COUNT; ++i)
	{
		Length d_i = Number(i) * dx;
		DimensionlessSpectrum rayleigh_i;
		DimensionlessSpectrum mie_i;
		ComputeSingleScatteringIntegrand(atmosphere, transmittance_texture,
			r, mu, mu_s, nu, d_i, ray_r_mu_intersects_ground, rayleigh_i, mie_i);
		Number weight_i = (i == 0 || i == SAMPLE_COUNT) ? 0.5 : 1.0;
		rayleigh_sum += rayleigh_i * weight_i;
		mie_sum += mie_i * weight_i;
	}
	rayleigh = rayleigh_sum * dx * atmosphere.solar_irradiance * atmosphere.rayleigh_scattering;
	mie = mie_sum * dx * atmosphere.solar_irradiance * atmosphere.mie_scattering;
}

InverseSolidAngle RayleighPhaseFunction(Number nu)
{
	InverseSolidAngle k = 3.0 / (16.0 * PI * sr);
	return k * (1.0 + nu * nu);
}

InverseSolidAngle MiePhaseFunction(Number g, Number nu)
{
	InverseSolidAngle k = 3.0 / (8.0 * PI * sr) * (1.0 - g * g) / (2.0 + g * g);
	return k * (1.0 + nu * nu) / pow(1.0 + g * g - 2.0 * g * nu, 1.5);
}

// map r -> z, when r == bottom_radius, rho = 0; when r == top_radius, rho = H
// map mu -> w, when w belongs [0.5, 1], ray_mu_r not intersects ground; when w belongs [0, 0.5], ray_mu_r intersects ground
// map mu_s -> v
// map nu -> u
float4 GetScatteringTextureUvwzFromRMuMuSNu(
	const in AtmosphereParameters atmosphere,
	Length r,
	Number mu,
	Number mu_s,
	Number nu,
	bool ray_r_mu_intersects_ground
)
{
	// assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);
	// assert(mu >= -1.0 && mu <= 1.0);
	// assert(mu_s >= -1.0 && mu_s <= 1.0);
	// assert(nu >= -1.0 && nu <= 1.0);

	// map r -> z
	Length H = sqrt(atmosphere.top_radius * atmosphere.top_radius - atmosphere.bottom_radius * atmosphere.bottom_radius);
	Length rho = SafeSqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);
	Number u_r = GetTextureCoordFromUnitRange(rho / H, SCATTERING_TEXTURE_R_SIZE);

	// map mu -> w
	Length r_mu = r * mu;
	Area discriminant = r_mu * r_mu - r * r + atmosphere.bottom_radius * atmosphere.bottom_radius;
	Number u_mu;
	if (ray_r_mu_intersects_ground)
	{
		Length d = -r_mu - SafeSqrt(discriminant);
		Length d_min = r - atmosphere.bottom_radius;
		Length d_max = rho;
		u_mu = 0.5 - 0.5 * GetTextureCoordFromUnitRange(d_max == d_min ? 0.0 :
			(d - d_min) / (d_max - d_min), SCATTERING_TEXTURE_MU_SIZE / 2);
	}
	else
	{
		Length d = -r_mu + SafeSqrt(discriminant + H * H);
		Length d_min = atmosphere.top_radius - r;
		Length d_max = rho + H;
		u_mu = 0.5 + 0.5 * GetTextureCoordFromUnitRange((d - d_min) / (d_max - d_min), SCATTERING_TEXTURE_MU_SIZE / 2);
	}

	// map mu_s -> v
	Length d = DistanceToTopAtmosphereBoundary(atmosphere, atmosphere.bottom_radius, mu_s);
	Length d_min = atmosphere.top_radius - atmosphere.bottom_radius;
	Length d_max = H;
	Number a = (d - d_min) / (d_max - d_min);
	Number A = -2.0 * atmosphere.mu_s_min * atmosphere.bottom_radius / (d_max - d_min);
	Number u_mu_s = GetTextureCoordFromUnitRange(max(1.0 - a / A, 0.0) / (1.0 + a), SCATTERING_TEXTURE_MU_S_SIZE);

	// map nu -> u
	Number u_nu = (nu + 1.0) / 2.0;
	return float4(u_nu, u_mu_s, u_mu, u_r);
}

void GetRMuMuSNuFromScatteringTextureUvwz(
	const in AtmosphereParameters atmosphere,
	const in float4 uvwz,
	out Length r,
	out Number mu,
	out Number mu_s,
	out Number nu,
	out bool ray_r_mu_intersects_ground
)
{
	// assert(uvwz.x >= 0.0 && uvwz.x <= 1.0);
	// assert(uvwz.y >= 0.0 && uvwz.y <= 1.0);
	// assert(uvwz.z >= 0.0 && uvwz.z <= 1.0);
	// assert(uvwz.w >= 0.0 && uvwz.w <= 1.0);

	Length H = sqrt(atmosphere.top_radius * atmosphere.top_radius - atmosphere.bottom_radius * atmosphere.bottom_radius);
	Length rho = H * GetUnitRangeFromTextureCoord(uvwz.w, SCATTERING_TEXTURE_R_SIZE);
	r = sqrt(rho * rho + atmosphere.bottom_radius * atmosphere.bottom_radius);

	if (uvwz.z < 0.5)
	{
		Length d_min = r - atmosphere.bottom_radius;
		Length d_max = rho;
		Length d = d_min + (d_max - d_min) * GetUnitRangeFromTextureCoord(1.0 - 2.0 * uvwz.z, SCATTERING_TEXTURE_MU_SIZE / 2);
		mu = d == 0.0 * m ? Number(-1.0) : ClampCosine(-(rho * rho + d * d) / (2.0 * r * d));
		ray_r_mu_intersects_ground = true;
	}
	else
	{
		Length d_min = atmosphere.top_radius - r;
		Length d_max = rho + H;
		Length d = d_min + (d_max - d_min) * GetUnitRangeFromTextureCoord(2.0 * uvwz.z - 1.0, SCATTERING_TEXTURE_MU_SIZE / 2);
		mu = d == 0.0 * m ? Number(1.0) : ClampCosine((H * H - rho * rho - d * d) / (2.0 * r * d));
		ray_r_mu_intersects_ground = false;
	}

	Number x_mu_s = GetUnitRangeFromTextureCoord(uvwz.y, SCATTERING_TEXTURE_MU_S_SIZE);
	Length d_min = atmosphere.top_radius - atmosphere.bottom_radius;
	Length d_max = H;
	Number A = -2.0 * atmosphere.mu_s_min * atmosphere.bottom_radius / (d_max - d_min);
	Number a = (A - x_mu_s * A) / (1.0 + x_mu_s * A);
	Length d = d_min + min(a, A) * (d_max - d_min);
	mu_s = d == 0.0 * m ? Number(1.0) : ClampCosine((H * H - d * d) / (2.0 * atmosphere.bottom_radius * d));

	nu = ClampCosine(uvwz.x * 2.0 - 1.0);
}

void GetRMuMuSNuFromScatteringTextureFragCoord(
	const in AtmosphereParameters atmosphere,
	const in float3 frag_coord,
	out Length r,
	out Number mu,
	out Number mu_s,
	out Number nu,
	out bool ray_r_mu_intersects_ground
)
{
	const float4 SCATTERING_TEXTURE_SIZE = float4(
		SCATTERING_TEXTURE_NU_SIZE - 1,
		SCATTERING_TEXTURE_MU_S_SIZE,
		SCATTERING_TEXTURE_MU_SIZE,
		SCATTERING_TEXTURE_R_SIZE
	);
	Number frag_coord_nu = floor(frag_coord.x / Number(SCATTERING_TEXTURE_MU_S_SIZE));
	Number frag_coord_mu_s = fmod(frag_coord.x, Number(SCATTERING_TEXTURE_MU_S_SIZE));
	float4 uvwz = float4(frag_coord_nu, frag_coord_mu_s, frag_coord.y, frag_coord.z) / SCATTERING_TEXTURE_SIZE;
	GetRMuMuSNuFromScatteringTextureUvwz(atmosphere, uvwz, r, mu, mu_s, nu, ray_r_mu_intersects_ground);
	nu = clamp(nu, mu * mu_s - sqrt((1.0 - mu * mu) * (1.0 - mu_s * mu_s)), mu * mu_s + sqrt((1.0 - mu * mu) * (1.0 - mu_s * mu_s)));
}

void ComputeSingleScatteringTexture(
	const in AtmosphereParameters atmosphere,
	const in TransmittanceTexture transmittance_texture,
	const in float3 frag_coord,
	out IrradianceSpectrum rayleigh,
	out IrradianceSpectrum mie
)
{
	Length r;
	Number mu;
	Number mu_s;
	Number nu;
	bool ray_r_mu_intersects_ground;
	GetRMuMuSNuFromScatteringTextureFragCoord(atmosphere, frag_coord, r, mu, mu_s, nu, ray_r_mu_intersects_ground);
	ComputeSingleScattering(atmosphere, transmittance_texture, r, mu, mu_s, nu, ray_r_mu_intersects_ground, rayleigh, mie);
}

// ******************* Single scattering lookup ******************* //

// use r, mu, mu_s, nu get single scattering from precompute texture
AbstractSpectrum GetScattering(
	const in AtmosphereParameters atmosphere,
	const in AbstractScatteringTexture scattering_texture,
	Length r,
	Number mu,
	Number mu_s,
	Number nu,
	bool ray_r_mu_intersects_ground
)
{
	float4 uvwz = GetScatteringTextureUvwzFromRMuMuSNu(
		atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground
	);
	Number tex_coord_x = uvwz.x * Number(SCATTERING_TEXTURE_NU_SIZE - 1);
	Number tex_x = floor(tex_coord_x);
	Number lerp = tex_coord_x - tex_x;
	float3 uvw0 = float3((tex_x + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE), uvwz.z, uvwz.w);
	float3 uvw1 = float3((tex_x + 1.0 + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE), uvwz.z, uvwz.w);
	return AbstractSpectrum(scattering_texture.SampleLevel(LinearClampSampler, uvw0, 0.0).xyz * (1.0 - lerp) + scattering_texture.SampleLevel(LinearClampSampler, uvw1, 0.0).xyz * lerp);
}

// get single scattering or multiple scattering from precompute texture
RadianceSpectrum GetScattering(
	const in AtmosphereParameters atmosphere,
	const in ReducedScatteringTexture single_rayleigh_scattering_texture,
	const in ReducedScatteringTexture single_mie_scattering_texture,
	const in ScatteringTexture multiple_scattering_texture,
	Length r,
	Number mu,
	Number mu_s,
	Number nu,
	bool ray_r_mu_intersects_ground,
	int scattering_order
)
{
	if (scattering_order == 1)
	{
		IrradianceSpectrum rayleigh = GetScattering(
			atmosphere, single_rayleigh_scattering_texture, r, mu, mu_s, nu,
			ray_r_mu_intersects_ground
		);
		IrradianceSpectrum mie = GetScattering(
			atmosphere, single_mie_scattering_texture, r, mu, mu_s, nu,
			ray_r_mu_intersects_ground
		);
		return rayleigh * RayleighPhaseFunction(nu) +
			mie * MiePhaseFunction(atmosphere.mie_phase_function_g, nu);
	}
	else
	{
		return GetScattering(atmosphere, multiple_scattering_texture, r, mu, mu_s, nu, ray_r_mu_intersects_ground);
	}
}

// ******************* Multiple scattering precompute ******************* //

IrradianceSpectrum GetIrradiance(
	const in AtmosphereParameters atmosphere,
	const in IrradianceTexture irradiance_texture,
	Length r,
	Number mu_s
);

// First step of compute multipule scattering from (scattering_order - 1)th texture, 
// single scattering texture, irradiance texture, r, mu, mu_s, nu
// compute the scattering at some pos q towards a certain direction ray_r_mu, 
// by intergal over all the possible incident direction omega_i.
// the irradiance from omega_i equals the n-1 th bounce + the irradiance from the ground(if omega_i intersects the ground)
RadianceDensitySpectrum ComputeScatteringDensity(
	const in AtmosphereParameters atmosphere,
	const in TransmittanceTexture transmittance_texture,
	const in ReducedScatteringTexture single_rayleigh_scattering_texture,
	const in ReducedScatteringTexture single_mie_scattering_texture,
	const in ScatteringTexture multiple_scattering_texture,
	const in IrradianceTexture irradiance_texture,
	Length r,
	Number mu,
	Number mu_s,
	Number nu,
	int scattering_order
)
{
	// assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);
	// assert(mu >= -1.0 && mu <= 1.0);
	// assert(mu_s >= -1.0 && mu_s <= 1.0);
	// assert(nu >= -1.0 && nu <= 1.0);
	// assert(scattering_order >= 2);

	// Compute unit direction vectors
	// \vec{omega_s} \dot \vec{omega} = nu
	// \vec{omega} \dot \vec{zenith} = mu
	// \vec{omega_s} \ dot \vec{zenith} = mu_s

	// zenith
	float3 zenith_direction = float3(0.0, 0.0, 1.0);
	// view direction
	float3 omega = float3(sqrt(1.0 - mu * mu), 0.0, mu);
	Number sun_dir_x = omega.x == 0.0 ? 0.0 : (nu - mu * mu_s) / omega.x;
	Number sun_dir_y = sqrt(max(1.0 - sun_dir_x * sun_dir_x - mu_s * mu_s, 0.0));
	// sun direction
	float3 omega_s = float3(sun_dir_x, sun_dir_y, mu_s);

	const int SAMPLE_COUNT = 16;
	const Angle dphi = pi / Number(SAMPLE_COUNT);
	const Angle dtheta = pi / Number(SAMPLE_COUNT);
	RadianceDensitySpectrum rayleigh_mie =
		0.0 * watt_per_cubic_meter_per_sr_per_nm;

	for (int l = 0; l < SAMPLE_COUNT; ++l)
	{
		Angle theta = (Number(l) + 0.5) * dtheta;
		Number cos_theta = cos(theta);
		Number sin_theta = sin(theta);
		bool ray_r_theta_intersects_ground = RayIntersectsGround(atmosphere, r, cos_theta);

		Length distance_to_ground = 0.0 * m;
		DimensionlessSpectrum transmittance_to_ground = 0.0;
		DimensionlessSpectrum ground_albedo = 0.0;
		if (ray_r_theta_intersects_ground)
		{
			distance_to_ground = DistanceToBottomAtmosphereBoundary(atmosphere, r, cos_theta);
			transmittance_to_ground = GetTransmittance(atmosphere, transmittance_texture, r, cos_theta, distance_to_ground, true);
			ground_albedo = atmosphere.ground_albedo;
		}

		for (int m = 0; m < 2 * SAMPLE_COUNT; ++m)
		{
			Angle phi = (Number(m) + 0.5) * dphi;
			float3 omega_i = float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
			SolidAngle domega_i = (dtheta / rad) * (dphi / rad) * sin(theta) * sr;

			Number nu1 = dot(omega_s, omega_i);
			// The radiance L_i arriving from direction omega_i after n-1 bounces is
			// the sum of a term given by the precomputed scattering texture for the
			// (n-1)-th order:
			RadianceSpectrum incident_radiance = GetScattering(atmosphere,
				single_rayleigh_scattering_texture, single_mie_scattering_texture,
				multiple_scattering_texture, r, omega_i.z, mu_s, nu1,
				ray_r_theta_intersects_ground, scattering_order - 1);

			// and of the contribution from the light paths with n-1 bounces and whose
			// last bounce is on the ground. This contribution is the product of the
			// transmittance to the ground, the ground albedo, the ground BRDF, and
			// the irradiance received on the ground after n-2 bounces.
			float3 ground_normal = normalize(zenith_direction * r + omega_i * distance_to_ground);
			IrradianceSpectrum ground_irradiance = GetIrradiance(
				atmosphere, irradiance_texture, atmosphere.bottom_radius,
				dot(ground_normal, omega_s));
			incident_radiance += transmittance_to_ground * ground_albedo * (1.0 / (PI * sr)) * ground_irradiance;

			// The radiance finally scattered from direction omega_i towards direction
			// -omega is the product of the incident radiance, the scattering
			// coefficient, and the phase function for directions omega and omega_i
			// (all this summed over all particle types, i.e. Rayleigh and Mie).
			Number nu2 = dot(omega, omega_i);
			Number rayleigh_density = GetDensity(atmosphere.rayleigh_density, r - atmosphere.bottom_radius);
			Number mie_density = GetDensity(atmosphere.mie_density, r - atmosphere.bottom_radius);
			rayleigh_mie += incident_radiance * (
				atmosphere.rayleigh_scattering * rayleigh_density * RayleighPhaseFunction(nu2) +
				atmosphere.mie_scattering * mie_density * MiePhaseFunction(atmosphere.mie_phase_function_g, nu2)
				) * domega_i;
		}
	}
	return rayleigh_mie;
}

// Second step of compute multiple scattering
// for each pos p and direction ray_r_mu, integral the n-th bounce radiance product the transmittance over
// all the points q betwenn p and the nearest atmosphere boundary
RadianceSpectrum ComputeMultipleScattering(
	const in AtmosphereParameters atmosphere,
	const in TransmittanceTexture transmittance_texture,
	const in ScatteringDensityTexture scattering_density_texture,
	Length r,
	Number mu,
	Number mu_s,
	Number nu,
	bool ray_r_mu_intersects_ground
)
{
	// assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);
	// assert(mu >= -1.0 && mu <= 1.0);
	// assert(mu_s >= -1.0 && mu_s <= 1.0);
	// assert(nu >= -1.0 && nu <= 1.0);

	const int SAMPLE_COUNT = 50;
	Length dx = DistanceToNearestAtmosphereBoundary(atmosphere, r, mu, ray_r_mu_intersects_ground) / Number(SAMPLE_COUNT);
	RadianceSpectrum rayleigh_mie_sum = 0.0 * watt_per_cubic_meter_per_sr_per_nm;
	for (int i = 0; i <= SAMPLE_COUNT; ++i)
	{
		Length d_i = Number(i) * dx;
		Length r_i = ClampRadius(atmosphere, sqrt(d_i * d_i + 2.0 * r * mu * d_i + r * r));
		Number mu_i = ClampCosine((r * mu + d_i) / r_i);
		Number mu_s_i = ClampCosine((r * mu_s + d_i * nu) / r_i);

		RadianceSpectrum rayleigh_mie_i =
			GetScattering(atmosphere, scattering_density_texture, r_i, mu_i, mu_s_i, nu, ray_r_mu_intersects_ground) *
			GetTransmittance(atmosphere, transmittance_texture, r, mu, d_i, ray_r_mu_intersects_ground) *
			dx;
		Number weight_i = (i == 0 || i == SAMPLE_COUNT) ? 0.5 : 1.0;
		rayleigh_mie_sum += rayleigh_mie_i * weight_i;
	}
	return rayleigh_mie_sum;
}

// use frag coord precompute multiple scattering texture(final)
RadianceDensitySpectrum ComputeScatteringDensityTexture(
	const in AtmosphereParameters atmosphere,
	const in TransmittanceTexture transmittance_texture,
	const in ReducedScatteringTexture single_rayleigh_scattering_texture,
	const in ReducedScatteringTexture single_mie_scattering_texture,
	const in ScatteringTexture multiple_scattering_texture,
	const in IrradianceTexture irradiance_texture,
	const in float3 frag_coord,
	int scattering_order
)
{
	Length r;
	Number mu;
	Number mu_s;
	Number nu;
	bool ray_r_mu_intersects_ground;
	GetRMuMuSNuFromScatteringTextureFragCoord(atmosphere, frag_coord,
		r, mu, mu_s, nu, ray_r_mu_intersects_ground);
	return ComputeScatteringDensity(atmosphere, transmittance_texture,
		single_rayleigh_scattering_texture, single_mie_scattering_texture,
		multiple_scattering_texture, irradiance_texture, r, mu, mu_s, nu,
		scattering_order);
}

RadianceSpectrum ComputeMultipleScatteringTexture(
	const in AtmosphereParameters atmosphere,
	const in TransmittanceTexture transmittance_texture,
	const in ScatteringDensityTexture scattering_density_texture,
	const in float3 frag_coord, out Number nu
)
{
	Length r;
	Number mu;
	Number mu_s;
	bool ray_r_mu_intersects_ground;
	GetRMuMuSNuFromScatteringTextureFragCoord(atmosphere, frag_coord,
		r, mu, mu_s, nu, ray_r_mu_intersects_ground);
	return ComputeMultipleScattering(atmosphere, transmittance_texture,
		scattering_density_texture, r, mu, mu_s, nu,
		ray_r_mu_intersects_ground);
}

// ************* precompute ground irradiance ************* //

// use r, mu_s and transmittance texture compute the direct irradiance from the ground
// direct irradiance means the incident light is from the sun
IrradianceSpectrum ComputeDirectIrradiance(
	const in AtmosphereParameters atmosphere,
	const in TransmittanceTexture transmittance_texture,
	Length r,
	Number mu_s
)
{
	// assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);
	// assert(mu_s >= -1.0 && mu_s <= 1.0);

	Number alpha_s = atmosphere.sun_angular_radius / rad;
	// Approximate average of the cosine factor mu_s over the visible fraction of the sun disc
	Number average_cosine_factor = mu_s < -alpha_s ? 0.0 :
		(mu_s > alpha_s ? mu_s : (mu_s + alpha_s) * (mu_s + alpha_s) / (4.0 * alpha_s));
	return atmosphere.solar_irradiance * GetTransmittanceToTopAtmosphereBoundary(atmosphere, transmittance_texture, r, mu_s)
		* average_cosine_factor;
}

// use r, mu_s and scattering texture compute the indirect irradiance 
// by integral over all the direction omega of the hemisphere of the product of 
// the radiance arriving from direction omega after n bounces and the cosine factor omega.z
IrradianceSpectrum ComputeIndirectIrradiance(
	const in AtmosphereParameters atmosphere,
	const in ReducedScatteringTexture single_rayleigh_scattering_texture,
	const in ReducedScatteringTexture single_mie_scattering_texture,
	const in ScatteringTexture multiple_scattering_texture,
	Length r,
	Number mu_s,
	int scattering_order
)
{
	// assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);
	// assert(mu_s >= -1.0 && mu_s <= 1.0);
	// assert(scattering_order >= 1);

	const int SAMPLE_COUNT = 32;
	const Angle dphi = pi / Number(SAMPLE_COUNT);
	const Angle dtheta = pi / Number(SAMPLE_COUNT);

	IrradianceSpectrum result = 0.0 * watt_per_cubic_meter_per_sr_per_nm;
	float3 omega_s = float3(sqrt(1.0 - mu_s * mu_s), 0.0, mu_s);
	for (int j = 0; j < SAMPLE_COUNT / 2; ++j)
	{
		Angle theta = (Number(j) + 0.5) * dtheta;
		for (int i = 0; i < SAMPLE_COUNT * 2; ++i)
		{
			Angle phi = (Number(i) + 0.5) * dphi;
			float3 omega = float3(cos(phi) * sin(theta), sin(phi) * sin(theta), cos(theta));
			SolidAngle domega = (dtheta / rad) * (dphi / rad) * sin(theta) * sr;

			Number nu = dot(omega, omega_s);
			result += GetScattering(atmosphere, single_rayleigh_scattering_texture, single_mie_scattering_texture, multiple_scattering_texture,
				r, omega.z, mu_s, nu, false, scattering_order)
				* omega.z * domega;
		}
	}
	return result;
}

float2 GetIrradianceTextureUvFromRMuS(
	const in AtmosphereParameters atmosphere,
	Length r,
	Number mu_s
)
{
	// assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);
	// assert(mu_s >= -1.0 && mu_s <= 1.0);

	Number x_r = (r - atmosphere.bottom_radius) / (atmosphere.top_radius - atmosphere.bottom_radius);
	Number x_mu_s = mu_s * 0.5 + 0.5;
	return float2(GetTextureCoordFromUnitRange(x_mu_s, IRRADIANCE_TEXTURE_WIDTH),
		GetTextureCoordFromUnitRange(x_r, IRRADIANCE_TEXTURE_HEIGHT));
}

void GetRMuSFromIrradianceTextureUv(
	const in AtmosphereParameters atmosphere,
	const in float2 uv,
	out Length r,
	out Number mu_s
)
{
	// assert(uv.x >= 0.0 && uv.x <= 1.0);
	// assert(uv.y >= 0.0 && uv.y <= 1.0);

	Number x_mu_s = GetUnitRangeFromTextureCoord(uv.x, IRRADIANCE_TEXTURE_WIDTH);
	Number x_r = GetUnitRangeFromTextureCoord(uv.y, IRRADIANCE_TEXTURE_HEIGHT);
	r = atmosphere.bottom_radius + x_r * (atmosphere.top_radius - atmosphere.bottom_radius);
	mu_s = ClampCosine(2.0 * x_mu_s - 1.0);
}

static const float2 IRRADIANCE_TEXTURE_SIZE = float2(IRRADIANCE_TEXTURE_WIDTH, IRRADIANCE_TEXTURE_HEIGHT);

IrradianceSpectrum ComputeDirectIrradianceTexture(
	const in AtmosphereParameters atmosphere,
	const in TransmittanceTexture transmittance_texture,
	const in float2 frag_coord
)
{
	Length r;
	Number mu_s;
	GetRMuSFromIrradianceTextureUv(atmosphere, frag_coord / IRRADIANCE_TEXTURE_SIZE, r, mu_s);
	return ComputeDirectIrradiance(atmosphere, transmittance_texture, r, mu_s);
}

IrradianceSpectrum ComputeIndirectIrradianceTexture(
	const in AtmosphereParameters atmosphere,
	const in ReducedScatteringTexture single_rayleigh_scattering_texture,
	const in ReducedScatteringTexture single_mie_scattering_texture,
	const in ScatteringTexture multiple_scattering_texture,
	const in float2 frag_coord,
	int scattering_order
)
{
	Length r;
	Number mu_s;
	GetRMuSFromIrradianceTextureUv(atmosphere, frag_coord / IRRADIANCE_TEXTURE_SIZE, r, mu_s);
	return ComputeIndirectIrradiance(atmosphere, single_rayleigh_scattering_texture, single_mie_scattering_texture, multiple_scattering_texture,
		r, mu_s, scattering_order);
}

IrradianceSpectrum GetIrradiance(
	const in AtmosphereParameters atmosphere,
	const in IrradianceTexture irradiance_texture,
	Length r,
	Number mu_s
)
{
	float2 uv = GetIrradianceTextureUvFromRMuS(atmosphere, r, mu_s);
	return IrradianceSpectrum(irradiance_texture.SampleLevel(LinearClampSampler, uv, 0.0).xyz);
}

#ifdef COMBINED_SCATTERING_TEXTURES
float3 GetExtrapolatedSingleMieScattering(
	const in AtmosphereParameters atmosphere,
	const in float4 scattering
)
{
	if (scattering.r == 0.0)
		return 0.0;
	return scattering.rgb * scattering.a / scattering.r *
		(atmosphere.rayleigh_scattering.r / atmosphere.mie_scattering.r) * 
		(atmosphere.mie_scattering / atmosphere.rayleigh_scattering);
}
#endif

// retrieve all the scattering components 
// (Rayleigh + multiple scattering on one side, 
// and single Mie scattering on the other side) 
IrradianceSpectrum GetCombinedScattering(
	const in AtmosphereParameters atmosphere,
	const in ReducedScatteringTexture scattering_texture,
	const in ReducedScatteringTexture single_mie_scattering_texture,
	Length r,
	Number mu,
	Number mu_s,
	Number nu,
	bool ray_r_mu_intersects_ground,
	out IrradianceSpectrum single_mie_scattering
)
{
	float4 uvwz = GetScatteringTextureUvwzFromRMuMuSNu(
		atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground
	);
	Number tex_coord_x = uvwz.x * Number(SCATTERING_TEXTURE_NU_SIZE - 1);
	Number tex_x = floor(tex_coord_x);
	Number lerp = tex_coord_x - tex_x;
	float3 uvw0 = float3((tex_x + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE),
		uvwz.z, uvwz.w);
	float3 uvw1 = float3((tex_x + 1.0 + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE),
		uvwz.z, uvwz.w);

#ifdef COMBINED_SCATTERING_TEXTURES
	float4 combined_scattering =
		scattering_texture.SampleLevel(LinearClampSampler, uvw0, 0.0) * (1.0 - lerp) +
		scattering_texture.SampleLevel(LinearClampSampler, uvw1, 0.0) * lerp;
	IrradianceSpectrum scattering = IrradianceSpectrum(combined_scattering);
	single_mie_scattering = GetExtrapolatedSingleMieScattering(atmosphere, combined_scattering);
#else
	IrradianceSpectrum scattering = IrradianceSpectrum(
		scattering_texture.SampleLevel(LinearClampSampler, uvw0, 0.0).xyz * (1.0 - lerp) +
		scattering_texture.SampleLevel(LinearClampSampler, uvw1, 0.0).xyz * lerp
	);
	single_mie_scattering = IrradianceSpectrum(
		single_mie_scattering_texture.SampleLevel(LinearClampSampler, uvw0, 0.0).xyz * (1.0 - lerp) +
		single_mie_scattering_texture.SampleLevel(LinearClampSampler, uvw1, 0.0).xyz * lerp
	);
#endif
	return scattering;
}


RadianceSpectrum GetSkyRadiance(
	const in AtmosphereParameters atmosphere,
	const in TransmittanceTexture transmittance_texture,
	const in ReducedScatteringTexture scattering_texture,
	const in ReducedScatteringTexture single_mie_scattering_texture,
	Position camera,
	const in Direction view_ray,
	Length shadow_length,
	const in Direction sun_direction,
	out DimensionlessSpectrum transmittance
)
{
	Length r = length(camera);
	Length rmu = dot(camera, view_ray);
	Length distance_to_top_atmosphere_boundary = -rmu -
		sqrt(rmu * rmu - r * r + atmosphere.top_radius * atmosphere.top_radius);

	if (distance_to_top_atmosphere_boundary > 0.0 * m)
	{
		camera = camera + view_ray * distance_to_top_atmosphere_boundary;
		r = atmosphere.top_radius;
		rmu += distance_to_top_atmosphere_boundary;
	}
	else if (r > atmosphere.top_radius)
	{
		transmittance = DimensionlessSpectrum(1.0, 1.0, 1.0);
		return 0.0 * watt_per_cubic_meter_per_sr_per_nm;
	}

	Number mu = rmu / r;
	Number mu_s = dot(camera, sun_direction) / r;
	Number nu = dot(view_ray, sun_direction);
	bool ray_r_mu_intersects_ground = RayIntersectsGround(atmosphere, r, mu);

	transmittance = ray_r_mu_intersects_ground ? DimensionlessSpectrum(0.0, 0.0, 0.0) :
		GetTransmittanceToTopAtmosphereBoundary(atmosphere, transmittance_texture, r, mu);
	IrradianceSpectrum single_mie_scattering;
	IrradianceSpectrum scattering;
	if (shadow_length == 0.0 * m)
	{
		scattering = GetCombinedScattering(
			atmosphere, scattering_texture, single_mie_scattering_texture,
			r, mu, mu_s, nu, ray_r_mu_intersects_ground,
			single_mie_scattering);
	}
	else
	{
		Length d = shadow_length;
		Length r_p = ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));
		Number mu_p = (r * mu + d) / r_p;
		Number mu_s_p = (r * mu_s + d * nu) / r_p;

		scattering = GetCombinedScattering(
			atmosphere, scattering_texture, single_mie_scattering_texture,
			r_p, mu_p, mu_s_p, nu, ray_r_mu_intersects_ground,
			single_mie_scattering
		);
		DimensionlessSpectrum shadow_transmittance =
			GetTransmittance(atmosphere, transmittance_texture,
				r, mu, shadow_length, ray_r_mu_intersects_ground);
		scattering = scattering * shadow_transmittance;
		single_mie_scattering = single_mie_scattering * shadow_transmittance;
	}
	return scattering * RayleighPhaseFunction(nu) + single_mie_scattering *
		MiePhaseFunction(atmosphere.mie_phase_function_g, nu);
}

RadianceSpectrum GetSkyRadianceToPoint(
	const in AtmosphereParameters atmosphere,
	const in TransmittanceTexture transmittance_texture,
	const in ReducedScatteringTexture scattering_texture,
	const in ReducedScatteringTexture single_mie_scattering_texture,
	Position camera,
	const in Position pos,
	Length shadow_length,
	const in Direction sun_direction,
	out DimensionlessSpectrum transmittance
)
{
	Direction view_ray = normalize(pos - camera);
	Length r = length(camera);
	Length rmu = dot(camera, view_ray);
	Length distance_to_top_atmosphere_boundary = -rmu -
		sqrt(rmu * rmu - r * r + atmosphere.top_radius * atmosphere.top_radius);

	if (distance_to_top_atmosphere_boundary > 0.0 * m)
	{
		camera = camera + view_ray * distance_to_top_atmosphere_boundary;
		r = atmosphere.top_radius;
		rmu += distance_to_top_atmosphere_boundary;
	}

	Number mu = rmu / r;
	Number mu_s = dot(camera, sun_direction) / r;
	Number nu = dot(view_ray, sun_direction);
	Length d = length(pos - camera);
	bool ray_r_mu_intersects_ground = RayIntersectsGround(atmosphere, r, mu);

	transmittance = GetTransmittance(atmosphere, transmittance_texture,
		r, mu, d, ray_r_mu_intersects_ground);

	IrradianceSpectrum single_mie_scattering;
	IrradianceSpectrum scattering = GetCombinedScattering(
		atmosphere, scattering_texture, single_mie_scattering_texture,
		r, mu, mu_s, nu, ray_r_mu_intersects_ground,
		single_mie_scattering
	);

	d = max(d - shadow_length, 0.0 * m);
	Length r_p = ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));
	Number mu_p = (r * mu + d) / r_p;
	Number mu_s_p = (r * mu_s + d * nu) / r_p;

	IrradianceSpectrum single_mie_scattering_p;
	IrradianceSpectrum scattering_p = GetCombinedScattering(
		atmosphere, scattering_texture, single_mie_scattering_texture,
		r_p, mu_p, mu_s_p, nu, ray_r_mu_intersects_ground,
		single_mie_scattering_p
	);

	DimensionlessSpectrum shadow_transmittance = transmittance;
	if (shadow_length > 0.0 * m)
	{
		shadow_transmittance = GetTransmittance(atmosphere, transmittance_texture,
			r, mu, d, ray_r_mu_intersects_ground);
	}
	scattering = scattering - shadow_transmittance * scattering_p;
	single_mie_scattering =
		single_mie_scattering - shadow_transmittance * single_mie_scattering_p;
#ifdef COMBINED_SCATTERING_TEXTURES
	single_mie_scattering = GetExtrapolatedSingleMieScattering(
		atmosphere, float4(scattering, single_mie_scattering.r)
	);
#endif
	single_mie_scattering = single_mie_scattering *
		smoothstep(Number(0.0), Number(0.01), mu_s);

	return scattering * RayleighPhaseFunction(nu) + single_mie_scattering *
		MiePhaseFunction(atmosphere.mie_phase_function_g, nu);
}

IrradianceSpectrum GetSunAndSkyIrradiance(
	const in AtmosphereParameters atmosphere,
	const in TransmittanceTexture transmittance_texture,
	const in IrradianceTexture irradiance_texture,
	const in Position pos,
	//const in Direction normal,
	const in Direction sun_direction,
	out IrradianceSpectrum sky_irradiance
)
{
	Length r = length(pos);
	Number mu_s = dot(pos, sun_direction) / r;
	sky_irradiance = GetIrradiance(atmosphere, irradiance_texture, r, mu_s);// *
		//(1.0 + dot(normal, pos) / r) * 0.5;

	return atmosphere.solar_irradiance * GetTransmittanceToSun(
		atmosphere, transmittance_texture, r, mu_s
	);// * max(dot(normal, sun_direction), 0.0);
}
