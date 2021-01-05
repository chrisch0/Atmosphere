cbuffer PassConstant : register(b0)
{
	float4x4 ViewMatrix;
	float4x4 ProjMatrix;
	float4x4 ViewProjMatrix;
	float4x4 InvViewMatrix;
	float4x4 InvProjMatrix;
	float4x4 InvViewProjMatrix;
	float3 ViewerPos;
	float Time;
}

Texture3D<float4> BasicCloudShape : register(t0);
Texture2D<float4> StratusGradient : register(t1);
Texture2D<float4> CumulusGradient : register(t2);
Texture2D<float4> CumulonimbusGradient : register(t3);

SamplerState LinearRepeatSampler : register(s0);

struct PSInput
{
	float4 position : SV_Position;
	float3 worldPos : WorldPos;
	float2 uv : Texcoord0;
	float3 viewDir : Texcoord1;
	float3 boxCenter : Texcoord2;
	float3 boxScale : Texcoord3;
	float3 boxMinWorld : Texcoord4;
	float3 boxMaxWorld : Texcorrd5;
};

float GetHeightFractionForPoint(float3 position, float2 cloudMinMax)
{
	float height_fraction = (position.y - cloudMinMax.x) / (cloudMinMax.y - cloudMinMax.x);
	return saturate(height_fraction);
}

float GetDensityHeightGradientForPoint(float3 position) // float3 weatherData
{
	float height_fraction = GetHeightFractionForPoint(position, float2(0.0f, 1.0f));
	return CumulusGradient.Sample(LinearRepeatSampler, float2(0.5f, 1.0f - height_fraction));
}

float GetDensityHeightGradientFromWorldPos(float3 posWS, float3 boxCenter, float3 boxScale)
{
	float height_fraction = (posWS.y - (boxCenter.y - 0.5 * boxScale.y)) / boxScale.y;
	return CumulonimbusGradient.Sample(LinearRepeatSampler, float2(0.5f, 1.0f - height_fraction));
}

void CalculateBoxIntersectStartEnd(float3 viewerPos, float3 dir, float3 boxMin, float3 boxMax, out float3 start, out float3 end)
{
	float3 inv_dir = 1.0f / dir;
	float3 p0 = (boxMin - viewerPos) * inv_dir;
	float3 p1 = (boxMax - viewerPos) * inv_dir;
	float3 t0 = min(p0, p1);
	float3 t1 = max(p0, p1);

	float t_min = max(t0.x, max(t0.y, t0.z));
	float t_max = min(t1.x, min(t1.y, t1.z));
	
	start = viewerPos + dir * t_min;
	end = viewerPos + dir * t_max;
}

float3 LocalPositionToUVW(float3 p, float3 boxScale)
{
	float3 box_min = float3(-0.5f, -0.5f, -0.5f) * boxScale;
	return (p - box_min) / boxScale;
}

float Remap(float originalValue, float originalMin, float originalMax,
	float newMin, float newMax)
{
	return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
}

float SampleCloudDensity(float3 p, float3 weather_data, bool simpleSample)
{
	float4 low_frequency_noises = BasicCloudShape.Sample(LinearRepeatSampler, p).rgba;
	float low_freq_FBM = 
		(low_frequency_noises.g * 0.625f) +
		(low_frequency_noises.b * 0.25f) +
		(low_frequency_noises.a * 0.125f);
	float base_cloud = Remap(low_frequency_noises.r, -(1.0f - low_freq_FBM), 1.0, 0.0, 1.0);
	
	//base_cloud *= density_height_gradient;
	
	return base_cloud;
}

float HenyeyGreenstein(float3 lightDir, float3 viewDir, float G)
{
	float cos_angle = dot(lightDir, viewDir);
	return ((1.0f - G * G) / pow((1.0f + G * G - 2.0f * G * cos_angle), 1.5f)) / (4.0f * 3.1415926f);
}

float4 Integrate(float4 sum, float dif, float den)
{
	float3 lin = float3(0.65f, 0.7f, 0.75f) * 1.4f + float3(1.0f, 0.6f, 0.3f) * dif;
	float4 col = float4(lerp(float3(1.0f, 0.95f, 0.8f), float3(0.25f, 0.3f, 0.35f), den), den);
	col.xyz *= lin;
	col.a *= 0.4f;
	col.rgb *= col.a;
	return sum + col * (1.0f - sum.a);
}

float4 Render(float3 lightDir, float3 lightColor, float3 start, float3 end, float3 center, float3 scale, const int densitySampleCount, const int shadowSampleCount)
{
	float step_size = 1.0f / densitySampleCount;

	float cur_density = 0.0f;
	float transmittance = 1.0f;
	float3 camera_vec = (end - start) * step_size;

	float shadow_step_size = 1.0f / shadowSampleCount;
	lightDir *= shadow_step_size;
	float shadow_density = 8.0f * shadow_step_size;

	float density = 18.0f * step_size;
	float3 light_energy = 0.0f;
	float shadow_thresh = -log(0.01f) / shadow_density;

	float3 cur_pos = start;

	for (int i = 0; i < densitySampleCount; ++i)
	{
		float3 uvw = LocalPositionToUVW(cur_pos - center, scale);
		//float2 cur_sample = SampleCloudDensity(uvw, float3(1.0f, 1.0f, 1.0f), false).xx;
		float2 cur_sample = BasicCloudShape.Sample(LinearRepeatSampler, uvw).rr;

		if (cur_sample.r + cur_sample.g > 0.001f)
		{
			float3 light_pos = cur_pos;
			float shadow_dist = 0.0f;

			for (int s = 0; s < shadowSampleCount; ++s)
			{
				light_pos += lightDir;
				float3 light_uvw = LocalPositionToUVW(light_pos - center, scale);
				//float light_sample = SampleCloudDensity(light_uvw, float3(1.0f, 1.0f, 1.0f), false);
				float3 shadow_box_test = floor(0.5 + (abs(0.5 - light_uvw)));
				float exit_shadow_box = shadow_box_test.x + shadow_box_test.y + shadow_box_test.z;
				if (shadow_dist > shadow_thresh || exit_shadow_box >= 1)
					break;
				
				float light_sample = BasicCloudShape.Sample(LinearRepeatSampler, light_uvw).r;

				shadow_dist += light_sample;
			}

			cur_density = saturate(cur_sample.r * density);
			light_energy += (exp(-shadow_dist * shadow_density) * cur_density * transmittance * lightColor);
			//light_energy += cur_sample.g * transmittance * float3(0.0f, 0.1f, 0.0f);

			transmittance *= (1.0f - cur_density);
		}
		cur_pos += camera_vec;
	}
	return float4(light_energy, transmittance);
}

float4 ShowVolumeTexture(float3 start, float3 end, float3 center, float3 scale)
{
	float3 view_dir = (end - start) / 64.0f;
	float3 pos = start;
	float color = 0.0;
	for (int i = 0; i < 64; ++i)
	{
		float3 uvw = LocalPositionToUVW(pos - center, scale);
		float cur_sample = BasicCloudShape.Sample(LinearRepeatSampler, uvw).r;
		//color = color * (cur_sample) + cur_sample * (1.0f - cur_sample);
		color += cur_sample / 64.0f;
		pos += view_dir;
	}

	return float4(color.xxx, 1.0);
}

float UVRandom(float2 uv)
{
	float f = dot(float2(12.9898, 78.233), uv);
	return frac(43758.5453 * sin(f));
}

float4 RenderCloud(PSInput psInput)
{
	float4 color = 0.0f;

	float3 ray = normalize(psInput.viewDir);
	int samples = 64;

	float3 start = 0.0f;
	float3 end = 0.0f;
	CalculateBoxIntersectStartEnd(ViewerPos, ray, psInput.boxMinWorld, psInput.boxMaxWorld, start, end);
	float stride = length(end - start) / samples;

	float3 light = normalize(float3(0.0f, -1.0f, 1.0f));
	float HGCoeff = 0.5f;
	float hg = HenyeyGreenstein(light, ray, HGCoeff);

	float2 uv = psInput.uv * Time;
	float offs = UVRandom(uv) * stride;

	float3 pos = start + ray * offs;
	float3 acc = 0.0f;

	float depth = 0.0;

	float Scatter = 0.009f;
	float3 LightColor = float3(1.0, 0.875, 0.55);
	[loop]
	for (int s = 0; s < samples; s++)
	{
		float density_height_gradient = GetDensityHeightGradientFromWorldPos(pos, psInput.boxCenter, psInput.boxScale);
		if (density_height_gradient > 0.01f)
		{
			/*float n = SampleCloudDensity(pos, float3(1.0f, 1.0f, 1.0f), false);
			if (n > 0.0)
			{
				float density = n * stride;
				float rand = UVRandom(uv + s + 1);
				float scatter = density * Scatter * hg * MarchLight(pos, rand * 0.5f);
				acc += LightColor * scatter * BeerPowder(depth);
				depth += density;
			}*/
		}
	}



	return color;
}

float4 main(PSInput psInput) : SV_Target
{
	float3 raymarch_start = ViewerPos + psInput.viewDir;
	float3 dir = normalize(psInput.viewDir);

	const int sample_count = 64;
	float3 start = 0.0f;
	float3 end = 0.0f;
	CalculateBoxIntersectStartEnd(ViewerPos, dir, psInput.boxMinWorld, psInput.boxMaxWorld, start, end);
	float step = length(end - start) / sample_count;

	float3 sun_dir = -normalize(float3(0.0f, -1.0f, 1.0f));
	float3 light_color = HenyeyGreenstein(-sun_dir, dir, 0.2f) * float3(1.0, 0.875, 0.55) * 10.0f;

	//float4 color = ShowVolumeTexture(start, end, psInput.boxCenter, psInput.boxScale);
	//return color;

	float4 color = Render(sun_dir, light_color, start, end, psInput.boxCenter, psInput.boxScale, 64, 16);
	return color;

	//float density = 0.0f;
	//float3 p = start;
	//float4 sum = 0.0f;

	//for (int i = 0; i < sample_count; ++i)
	//{
	//	float3 uvw = LocalPositionToUVW(p - psInput.boxCenter, psInput.boxScale) * 0.5f;
	//	density += SampleCloudDensity(uvw, float3(1.0f, 1.0f, 1.0f), false) * step;

	//	p += dir * step;

	//}
	//density /= length(psInput.boxScale);
	//if (density > 1.0f)
	//	density = exp(-density);
	////density *= 1.2f;

	//return float4(density, density, density, 1.0f);

}

//// PlaneAlignment 1
//// MaxSteps 64
//float scale = length(TransformLocalVectorToWorld(Parameters, float3(1.00000000, 0.00000000, 0.00000000)).xyz);
//float localscenedepth = CalcSceneDepth(ScreenAlignedPosition(GetScreenPosition(Parameters)));
//
//float3 camerafwd = mul(float3(0.00000000, 0.00000000, 1.00000000), ResolvedView.ViewToTranslatedWorld);
//localscenedepth /= (GetPrimitiveData(Parameters.PrimitiveId).LocalObjectBoundsMax.x * 2 * scale);
//localscenedepth /= abs(dot(camerafwd, Parameters.CameraVector));
//
////bring vectors into local space to support object transforms
//float3 localcampos = mul(float4(ResolvedView.WorldCameraOrigin, 1.00000000), (GetPrimitiveData(Parameters.PrimitiveId).WorldToLocal)).xyz;
//float3 localcamvec = -normalize(mul(Parameters.CameraVector, GetPrimitiveData(Parameters.PrimitiveId).WorldToLocal));
//
////make camera position 0-1
//localcampos = (localcampos / (GetPrimitiveData(Parameters.PrimitiveId).LocalObjectBoundsMax.x * 2)) + 0.5;
//
//float3 invraydir = 1 / localcamvec;
//
//float3 firstintersections = (0 - localcampos) * invraydir;
//float3 secondintersections = (1 - localcampos) * invraydir;
//float3 closest = min(firstintersections, secondintersections);
//float3 furthest = max(firstintersections, secondintersections);
//
//float t0 = max(closest.x, max(closest.y, closest.z));
//float t1 = min(furthest.x, min(furthest.y, furthest.z));
//
//float planeoffset = 1 - frac((t0 - length(localcampos - 0.5)) * MaxSteps);
//
//t0 += (planeoffset / MaxSteps) * PlaneAlignment;
//t1 = min(t1, localscenedepth);
//t0 = max(0, t0);
//
//float boxthickness = max(0, t1 - t0);
//
//float3 entrypos = localcampos + (max(0, t0) * localcamvec);
//
//return float4(entrypos, boxthickness);