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
}

cbuffer CloudParameterCB : register(b1)
{
	float3 LightColor;
	int SampleCountMin;

	int SampleCountMax;
	float CloudCoverage;
	float Exposure;
	float GroundAlbedo;

	float3 CloudBottomColor;
	float Crispiness;

	float Curliness;
	float EarthRadius;
	float CloudBottomRadius;
	float CloudTopRadius;


	float CloudSpeed;
	float DensityFactor;
	float Absorption;
	float HG0;

	float HG1;
	int EnablePowder;
	int EnableBeer;
	float RainAbsorption;

	float Eccentricity;
	float SliverIntensity;
	float SliverSpread;
	float Brightness;
}

#define INNER_RADIUS (EarthRadius + CloudBottomRadius)
#define OUTER_RADIUS (EarthRadius + CloudTopRadius)
#define CLOUDS_MIN_TRANSMITTANCE 1e-1
//#define LIGHT_DIR float3(-0.40825, 0.40825, 0.8165)
#define LIGHT_DIR LightDir
#define SUN_COLOR LightColor * float3(1.1, 1.1, 0.95)

static const float3 NoiseKernel[] =
{
	float3(0.38051305,  0.92453449, -0.02111345),
	float3(-0.50625799, -0.03590792, -0.86163418),
	float3(-0.32509218, -0.94557439,  0.01428793),
	float3(0.09026238, -0.27376545,  0.95755165),
	float3(0.28128598,  0.42443639, -0.86065785),
	float3(-0.16852403,  0.14748697,  0.97460106)
};

#define BAYER_FACTOR 1.0/16.0
static const float BayerFilter[] =
{
	0.0*BAYER_FACTOR, 8.0*BAYER_FACTOR, 2.0*BAYER_FACTOR, 10.0*BAYER_FACTOR,
	12.0*BAYER_FACTOR, 4.0*BAYER_FACTOR, 14.0*BAYER_FACTOR, 6.0*BAYER_FACTOR,
	3.0*BAYER_FACTOR, 11.0*BAYER_FACTOR, 1.0*BAYER_FACTOR, 9.0*BAYER_FACTOR,
	15.0*BAYER_FACTOR, 7.0*BAYER_FACTOR, 13.0*BAYER_FACTOR, 5.0*BAYER_FACTOR
};

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

float2 WorldViewDirToUV(float3 worldDir, float4x4 ViewProj)
{
	float4 ndc = mul(ViewProj, float4(worldDir, 0.0));
	ndc /= ndc.w;
	return ndc.xy * float2(0.5, -0.5) + 0.5;
}

bool RaySphereIntersection(float3 ro, float3 rd, float radius, out float3 startPos)
{
	float t;
	float3 sphere_center = float3(CameraPosition.x, -EarthRadius, CameraPosition.z);
	float radius_sqr = radius * radius;
	float3 L = ro - sphere_center;

	float a = dot(rd, rd);
	float b = 2.0 * dot(rd, L);
	float c = dot(L, L) - radius_sqr;

	float discr = b * b - 4.0 * a * c;
	if (discr < 0.0) return false;
	t = max(0.0, (-b + sqrt(discr)) / 2.0f);
	if (t == 0.0)
		return false;

	startPos = ro + rd * t;

	return true;
}

bool RayIntersectGround(float3 ro, float3 rd, float radius, out float3 startPos)
{
	float3 p = ro + float3(0.0, radius, 0.0);
	float p_dot_v = dot(rd, p);
	float p_dot_p = dot(p, p);
	float ray_earth_center_squared_distance = p_dot_p - p_dot_v * p_dot_v;
	float distance_to_intersection = -p_dot_v - sqrt(radius * radius - ray_earth_center_squared_distance);
	if (distance_to_intersection > 0.0)
	{
		startPos = ro + distance_to_intersection * rd;
		return true;
	}
	return false;
}

float GetHeightFraction(float3 pos)
{
	float3 sphere_center = float3(CameraPosition.x, -EarthRadius, CameraPosition.z);
	return (length(pos.y - sphere_center.y) - INNER_RADIUS) / (OUTER_RADIUS - INNER_RADIUS);
}

float2 WorldPosToUV(float3 pos)
{
	return pos.xz / OUTER_RADIUS + 0.5;
}

float Remap(float originalValue, float originalMin, float originalMax, float newMin, float newMax)
{
	return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
}

float HG(float cosTheta, float g)
{
	float g_sqrt = g * g;
	return (1.0 - g_sqrt) / pow(1.0 + g_sqrt - 2.0 * g * cosTheta, 1.5) * 0.079577471f;
}

float Beer(float d)
{
	return exp(-d * RainAbsorption);
}

float Powder(float d)
{
	return (1.0 - exp(-2.0 * d));
}

float Threshold(const float v, const float t)
{
	return v > t ? v : 0.0;
}

#define STRATUS_GRADIENT float4(0.0, 0.1, 0.2, 0.3)
#define STRATOCUMULUS_GRADIENT float4(0.02, 0.2, 0.48, 0.625)
#define CUMULUS_GRADIENT float4(0.00, 0.1625, 0.88, 0.98)

float GetHeightDensityForCloud(float heightFraction, float cloudType)
{
	float stratus_factor = 1.0 - saturate(cloudType * 2.0);
	float strato_cumulus_factor = 1.0 - abs(cloudType - 0.5) * 2.0;
	float cumulus_factor = saturate(cloudType - 0.5) * 2.0;

	float4 base_gradient = stratus_factor * STRATUS_GRADIENT + strato_cumulus_factor * STRATOCUMULUS_GRADIENT + cumulus_factor * CUMULUS_GRADIENT;

	return smoothstep(base_gradient.x, base_gradient.y, heightFraction) - smoothstep(base_gradient.z, base_gradient.w, heightFraction);
}

//static float3 WindDirection = normalize(float3(0.5, 0.0, 0.1));
static float3 WindDirection = normalize(float3(1.0, 0.0, 0.0));
#define CLOUD_TOP_OFFSET 750.0

float SampleCloudDensity(float3 p, bool expensive, uint lod)
{
	float height_fraction = GetHeightFraction(p);
	float3 animation = height_fraction * WindDirection * CLOUD_TOP_OFFSET + (WindDirection + float3(0.0, 0.1, 0.0)) * Time * CloudSpeed;
	float2 uv = WorldPosToUV(p);
	float2 moving_uv = WorldPosToUV(p + animation);

	if (height_fraction < 0.0 || height_fraction > 1.0)
		return 0.0;

	float4 low_frequency_noise = CloudShapeTexture.SampleLevel(LinearRepeatSampler, float3(uv.x * Crispiness, height_fraction, uv.y * Crispiness), lod);
	//float4 low_frequency_noise = CloudShapeTexture.SampleLevel(LinearRepeatSampler, float3((p + animation) / Crispiness), lod);
	float low_freq_FBM = dot(low_frequency_noise.gba, float3(0.625, 0.25, 0.125));
	float base_cloud = Remap(low_frequency_noise.r, -(1.0 - low_freq_FBM), 1.0, 0.0, 1.0);

	float density = GetHeightDensityForCloud(height_fraction, 1.0);
	base_cloud *= (density / height_fraction);

	float3 weather_data = WeatherTexture.SampleLevel(LinearRepeatSampler, moving_uv, 0.0).rgb;
	float cloud_coverage = weather_data.r * CloudCoverage;
	float base_cloud_with_coverage = Remap(base_cloud, cloud_coverage, 1.0, 0.0, 1.0);
	base_cloud_with_coverage *= cloud_coverage;

	if (expensive)
	{
		//float2 curl_noise = CurlNoise.SampleLevel(LinearRepeatSampler, (p.xy + animation.xy) * Crispiness, 0.0).rg;
		//p.xy += curl_noise * (1.0 - height_fraction) * 200.0;
		//moving_uv = WorldPosToUV(p + animation);

		float3 erode_cloud_noise = ErosionTexture.SampleLevel(LinearRepeatSampler, float3(moving_uv.x * Crispiness, height_fraction, moving_uv.y * Crispiness) * Curliness, lod).rgb;
		//float3 erode_cloud_noise = ErosionTexture.SampleLevel(LinearRepeatSampler, float3((p + animation) / Crispiness * 0.1) * Curliness, lod).rgb;
		float high_freq_FBM = dot(erode_cloud_noise.rgb, float3(0.625, 0.25, 0.125));
		float high_freq_noise_modifier = lerp(high_freq_FBM, 1.0 - high_freq_FBM, saturate(height_fraction * 10.0));

		base_cloud_with_coverage = base_cloud_with_coverage - high_freq_noise_modifier * (1.0 - base_cloud_with_coverage);

		base_cloud_with_coverage = Remap(base_cloud_with_coverage * 2.0, high_freq_noise_modifier * 0.2, 1.0, 0.0, 1.0);
	}

	return max(0.0, base_cloud_with_coverage);
}

float RaymarchLight(float3 o, float stepSize, float3 lightDir, float originalDensity, float lightDotEye, uint mipLevel)
{
	float3 start_pos = o;
	float ds = stepSize * 6.0;
	float3 ray_step = lightDir * ds;
	const float CONE_STEP = 1.0 / 6.0;
	float cone_radius = 1.0;
	float density = 0.0;
	float cone_density = 0.0;
	float inv_depth = 1.0 / ds;
	float sigma_ds = ds * Absorption;
	float3 pos;
	//float T = 1.0;
	float T = 0.0;

	for (uint i = 0; i < 6; ++i)
	{
		pos = start_pos + cone_radius * NoiseKernel[i] * float(i);
		float height_fraction = GetHeightFraction(pos);
		if (height_fraction >= 0.0)
		{
			uint mip_offset = uint(i * 0.5);
			float cloud_density = SampleCloudDensity(pos, density > 0.3, mipLevel + mip_offset);
			//float cloud_density = SampleCloudDensity(pos, true, mipLevel + mip_offset);
			if (cloud_density > 0.0)
			{
				T += cloud_density * sigma_ds;
				density += cloud_density;
			}
		}
		start_pos += ray_step;
		cone_radius += CONE_STEP;
	}
	return T;
}

float GetLightEnergy(float3 p, float heightFraction, float dl, float dsLoded, float phaseProbability, float cosAngle, float stepSize, float brightness)
{
	float primary_attenuation = exp(-dl);
	float secondary_attenuation = exp(-dl * 0.25) * 0.7;
	float attenuation_probability = max(Remap(cosAngle, 0.7, 1.0, secondary_attenuation, secondary_attenuation * 0.25), primary_attenuation);
	//float attenuation_probability = max(primary_attenuation, secondary_attenuation);

	float depth_probability = lerp(0.05 + pow(dsLoded, Remap(heightFraction, 0.3, 0.85, 0.5, 2.0)), 1.0, saturate(dl / stepSize));
	//float depth_probability = 0.05 + pow(dsLoded, Remap(heightFraction, 0.3, 0.85, 0.5, 2.0));
	//float vertical_probability = pow(Remap(heightFraction, 0.07, 0.14, 0.1, 1.0), 0.8);
	float in_scatter_probability = depth_probability;// *vertical_probability;

	float light_energy = attenuation_probability * in_scatter_probability * phaseProbability * brightness;
	return light_energy;
}

float4 RaymarchCloud(uint2 pixelCoord, float3 startPos, float3 endPos, float3 bg, out float4 cloudPos)
{
	float3 path = endPos - startPos;
	float len = length(path);

	float3 dir = normalize(path);
	const int nSteps = lerp(SampleCountMin, SampleCountMax, dir.y);

	float ds = len / nSteps;
	//float3 dir = path / len;
	dir *= ds;
	float4 color = 0.0;
	uint a = pixelCoord.x % 4;
	uint b = pixelCoord.y % 4;
	startPos += dir * BayerFilter[a * 4 + b];
	float3 pos = startPos;

	float density = 0.0;
	float light_dot_eye = dot(normalize(LIGHT_DIR), normalize(dir));
	float T = 1.0;
	float sigma_ds = -ds * DensityFactor;
	bool expensive = true;
	bool entered = false;

	float cloud_test = 0.0;
	float previous_density_sample = -1.0;
	int zero_density_sample_count = 0;

	int low_lod_steps = nSteps / 8;
	float big_step = len / low_lod_steps;
	float3 low_lod_pos = pos;
	float ds_lodded = 0.0;
	float3 big_step_dir = normalize(path) * big_step;
	for (uint ii = 0; ii < low_lod_steps; ++ii)
	{
		uint mip = ii * 0.25;
		mip += 4;
		float density = SampleCloudDensity(low_lod_pos, false, mip);
		ds_lodded += density * big_step;
		low_lod_pos += big_step_dir;
	}

	float scattering = lerp(HG(light_dot_eye, HG0), HG(light_dot_eye, HG1), saturate(light_dot_eye * 0.5 + 0.5));
	scattering = max(scattering, 1.0);
	float phase_probability = max(HG(light_dot_eye, Eccentricity), SliverIntensity * HG(light_dot_eye, 0.99 - SliverSpread));

	for (uint i = 0; i < nSteps; ++i)
	{
		uint mip_level = i * 0.0625;
		float density_sample = SampleCloudDensity(pos, true, mip_level);

		if (density_sample > 0.0)
		{
			if (!entered)
			{
				cloudPos = float4(pos, 1.0);
				entered = true;
			}
			float height = GetHeightFraction(pos);
			float3 ambient_light = CloudBottomColor;
			float light_density = RaymarchLight(pos, ds * 0.1, LIGHT_DIR, density_sample, light_dot_eye, mip_level);
			float dTrans = exp(density_sample * sigma_ds);

			float powder_term = EnablePowder ? Powder(light_density) : 1.0f;
			float beer_term = EnableBeer ? 2.0f * Beer(light_density) : 1.0f;
			float3 S = 0.6 * (lerp(lerp(ambient_light * 1.8, bg, 0.2), scattering * SUN_COLOR, beer_term * powder_term * exp(-light_density))) * density_sample;
			float3 Sint = (S - S * dTrans) * (1.0 / density_sample);
			color.rgb += T * Sint;

			//float light_energy = GetLightEnergy(pos, height, light_density, ds_lodded, phase_probability, light_dot_eye, ds, Brightness);
			//color.rgb += light_energy * SUN_COLOR * T;

			T *= dTrans;
		}
		pos += dir;

		//if (cloud_test > 0.0)
		//{
		//	float density_sample = SampleCloudDensity(pos, true, 0.0);
		//	if (density_sample > 0.0)
		//	{
		//		if (!entered)
		//		{
		//			cloudPos = float4(pos, 1.0);
		//			entered = true;
		//		}

		//		float height = GetHeightFraction(pos);
		//		float3 ambient_light = CloudBottomColor;
		//		float light_density = RaymarchLight(pos, ds * 0.1, LIGHT_DIR, density_sample, light_dot_eye);
		//		float scattering = lerp(HG(light_dot_eye, HG0), HG(light_dot_eye, HG1), saturate(light_dot_eye * 0.5 + 0.5));
		//		scattering = max(scattering, 1.0);
		//		float powder_term = EnablePowder ? Powder(light_density) : 1.0f;
		//		float beer_term = EnableBeer ? 2.0f * Beer(light_density) : 1.0f;

		//		float3 S = 0.6 * (lerp(lerp(ambient_light * 1.8, bg, 0.2), scattering * SUN_COLOR, beer_term * powder_term * exp(-light_density))) * density_sample;
		//		float dTrans = exp(density_sample * sigma_ds);
		//		float3 Sint = (S - S * dTrans) * (1.0 / density_sample);
		//		color.rgb += T * Sint;
		//		T *= dTrans;

		//	}
		//	pos += dir;
		//}
		//else
		//{
		//	cloud_test = SampleCloudDensity(pos, false, 0.0);
		//	if (cloud_test == 0.0)
		//	{
		//		pos += dir * 2;
		//		i++;
		//	}
		//	else
		//	{
		//		pos -= dir;
		//		//i--;
		//	}
		//}

		if (T < CLOUDS_MIN_TRANSMITTANCE)
			break;

	}
	color.a = 1.0 - T;
	return color;
}