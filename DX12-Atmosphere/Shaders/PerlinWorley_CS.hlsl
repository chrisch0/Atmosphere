RWTexture3D<float4> PerlinWorleyTexture : register(u0);

static const float frequenceMul[6u] = { 2.0, 8.0, 14.0, 20.0, 26.0, 32.0 };

float hash(int n)
{
	return frac(sin(float(n) + 1.951) * 43758.5453123);
}

float noise(float3 x)
{
	float3 p = floor(x);
	float3 f = frac(x);

	f = f * f*(3.0 - 2.0 * f);
	float n = p.x + p.y*57.0 + 113.0*p.z;
	return lerp(
		lerp(
			lerp(hash(int(n + 0.0)), hash(int(n + 1.0)), f.x),
			lerp(hash(int(n + 57.0)), hash(int(n + 58.0)), f.x),
			f.y),
		lerp(
			lerp(hash(int(n + 113.0)), hash(int(n + 114.0)), f.x),
			lerp(hash(int(n + 170.0)), hash(int(n + 171.0)), f.x),
			f.y),
		f.z);
}

float cells(float3 p, float cellCount)
{
	float3 pCell = p * cellCount;
	float d = 1.0e10;
	for (int xo = -1; xo <= 1; xo++)
	{
		for (int yo = -1; yo <= 1; yo++)
		{
			for (int zo = -1; zo <= 1; zo++)
			{
				float3 tp = floor(pCell) + float3(xo, yo, zo);

				tp = pCell - tp - noise(fmod(tp, cellCount / 1.0));

				d = min(d, dot(tp, tp));
			}
		}
	}
	d = min(d, 1.0);
	d = max(d, 0.0f);

	return d;
}


// From GLM (gtc/noise.hpp & detail/_noise.hpp)
float4 mod289(float4 x)
{
	return x - floor(x * 1.0 / 289.0) * 289.0;
}

float4 permute(float4 x)
{
	return mod289(((x * 34.0) + 1.0) * x);
}

float4 taylorInvSqrt(float4 r)
{
	return 1.79284291400159 - 0.85373472095314 * r;
}

float4 fade(float4 t)
{
	return (t * t * t) * (t * (t * 6.0 - 15.0) + 10.0);
}

float glmPerlin4D(float4 Position, float4 rep)
{
	float4 Pi0 = fmod(floor(Position), rep);	// Integer part for indexing
	float4 Pi1 = fmod(Pi0 + 1.0, rep);		// Integer part + 1
	//Pi0 = fmod(Pi0, float4(289));
	//Pi1 = fmod(Pi1, float4(289));
	float4 Pf0 = frac(Position);	// Fractional part for interpolation
	float4 Pf1 = Pf0 - 1.0;		// Fractional part - 1.0
	float4 ix = float4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
	float4 iy = float4(Pi0.y, Pi0.y, Pi1.y, Pi1.y);
	float4 iz0 = Pi0.z;
	float4 iz1 = Pi1.z;
	float4 iw0 = Pi0.w;
	float4 iw1 = Pi1.w;

	float4 ixy = permute(permute(ix) + iy);
	float4 ixy0 = permute(ixy + iz0);
	float4 ixy1 = permute(ixy + iz1);
	float4 ixy00 = permute(ixy0 + iw0);
	float4 ixy01 = permute(ixy0 + iw1);
	float4 ixy10 = permute(ixy1 + iw0);
	float4 ixy11 = permute(ixy1 + iw1);

	float4 gx00 = ixy00 / 7.0;
	float4 gy00 = floor(gx00) / 7.0;
	float4 gz00 = floor(gy00) / 6.0;
	gx00 = frac(gx00) - 0.5;
	gy00 = frac(gy00) - 0.5;
	gz00 = frac(gz00) - 0.5;
	float4 gw00 = 0.75 - abs(gx00) - abs(gy00) - abs(gz00);
	float4 sw00 = step(gw00, 0.0);
	gx00 -= sw00 * (step(0.0, gx00) - 0.5);
	gy00 -= sw00 * (step(0.0, gy00) - 0.5);

	float4 gx01 = ixy01 / 7.0;
	float4 gy01 = floor(gx01) / 7.0;
	float4 gz01 = floor(gy01) / 6.0;
	gx01 = frac(gx01) - 0.5;
	gy01 = frac(gy01) - 0.5;
	gz01 = frac(gz01) - 0.5;
	float4 gw01 = 0.75 - abs(gx01) - abs(gy01) - abs(gz01);
	float4 sw01 = step(gw01, 0.0);
	gx01 -= sw01 * (step(0.0, gx01) - 0.5);
	gy01 -= sw01 * (step(0.0, gy01) - 0.5);

	float4 gx10 = ixy10 / 7.0;
	float4 gy10 = floor(gx10) / 7.0;
	float4 gz10 = floor(gy10) / 6.0;
	gx10 = frac(gx10) - 0.5;
	gy10 = frac(gy10) - 0.5;
	gz10 = frac(gz10) - 0.5;
	float4 gw10 = 0.75 - abs(gx10) - abs(gy10) - abs(gz10);
	float4 sw10 = step(gw10, 0.0);
	gx10 -= sw10 * (step(0.0, gx10) - 0.5);
	gy10 -= sw10 * (step(0.0, gy10) - 0.5);

	float4 gx11 = ixy11 / 7.0;
	float4 gy11 = floor(gx11) / 7.0;
	float4 gz11 = floor(gy11) / 6.0;
	gx11 = frac(gx11) - 0.5;
	gy11 = frac(gy11) - 0.5;
	gz11 = frac(gz11) - 0.5;
	float4 gw11 = 0.75 - abs(gx11) - abs(gy11) - abs(gz11);
	float4 sw11 = step(gw11, 0.0);
	gx11 -= sw11 * (step(0.0, gx11) - 0.5);
	gy11 -= sw11 * (step(0.0, gy11) - 0.5);

	float4 g0000 = float4(gx00.x, gy00.x, gz00.x, gw00.x);
	float4 g1000 = float4(gx00.y, gy00.y, gz00.y, gw00.y);
	float4 g0100 = float4(gx00.z, gy00.z, gz00.z, gw00.z);
	float4 g1100 = float4(gx00.w, gy00.w, gz00.w, gw00.w);
	float4 g0010 = float4(gx10.x, gy10.x, gz10.x, gw10.x);
	float4 g1010 = float4(gx10.y, gy10.y, gz10.y, gw10.y);
	float4 g0110 = float4(gx10.z, gy10.z, gz10.z, gw10.z);
	float4 g1110 = float4(gx10.w, gy10.w, gz10.w, gw10.w);
	float4 g0001 = float4(gx01.x, gy01.x, gz01.x, gw01.x);
	float4 g1001 = float4(gx01.y, gy01.y, gz01.y, gw01.y);
	float4 g0101 = float4(gx01.z, gy01.z, gz01.z, gw01.z);
	float4 g1101 = float4(gx01.w, gy01.w, gz01.w, gw01.w);
	float4 g0011 = float4(gx11.x, gy11.x, gz11.x, gw11.x);
	float4 g1011 = float4(gx11.y, gy11.y, gz11.y, gw11.y);
	float4 g0111 = float4(gx11.z, gy11.z, gz11.z, gw11.z);
	float4 g1111 = float4(gx11.w, gy11.w, gz11.w, gw11.w);

	float4 norm00 = taylorInvSqrt(float4(dot(g0000, g0000), dot(g0100, g0100), dot(g1000, g1000), dot(g1100, g1100)));
	g0000 *= norm00.x;
	g0100 *= norm00.y;
	g1000 *= norm00.z;
	g1100 *= norm00.w;

	float4 norm01 = taylorInvSqrt(float4(dot(g0001, g0001), dot(g0101, g0101), dot(g1001, g1001), dot(g1101, g1101)));
	g0001 *= norm01.x;
	g0101 *= norm01.y;
	g1001 *= norm01.z;
	g1101 *= norm01.w;

	float4 norm10 = taylorInvSqrt(float4(dot(g0010, g0010), dot(g0110, g0110), dot(g1010, g1010), dot(g1110, g1110)));
	g0010 *= norm10.x;
	g0110 *= norm10.y;
	g1010 *= norm10.z;
	g1110 *= norm10.w;

	float4 norm11 = taylorInvSqrt(float4(dot(g0011, g0011), dot(g0111, g0111), dot(g1011, g1011), dot(g1111, g1111)));
	g0011 *= norm11.x;
	g0111 *= norm11.y;
	g1011 *= norm11.z;
	g1111 *= norm11.w;

	float n0000 = dot(g0000, Pf0);
	float n1000 = dot(g1000, float4(Pf1.x, Pf0.y, Pf0.z, Pf0.w));
	float n0100 = dot(g0100, float4(Pf0.x, Pf1.y, Pf0.z, Pf0.w));
	float n1100 = dot(g1100, float4(Pf1.x, Pf1.y, Pf0.z, Pf0.w));
	float n0010 = dot(g0010, float4(Pf0.x, Pf0.y, Pf1.z, Pf0.w));
	float n1010 = dot(g1010, float4(Pf1.x, Pf0.y, Pf1.z, Pf0.w));
	float n0110 = dot(g0110, float4(Pf0.x, Pf1.y, Pf1.z, Pf0.w));
	float n1110 = dot(g1110, float4(Pf1.x, Pf1.y, Pf1.z, Pf0.w));
	float n0001 = dot(g0001, float4(Pf0.x, Pf0.y, Pf0.z, Pf1.w));
	float n1001 = dot(g1001, float4(Pf1.x, Pf0.y, Pf0.z, Pf1.w));
	float n0101 = dot(g0101, float4(Pf0.x, Pf1.y, Pf0.z, Pf1.w));
	float n1101 = dot(g1101, float4(Pf1.x, Pf1.y, Pf0.z, Pf1.w));
	float n0011 = dot(g0011, float4(Pf0.x, Pf0.y, Pf1.z, Pf1.w));
	float n1011 = dot(g1011, float4(Pf1.x, Pf0.y, Pf1.z, Pf1.w));
	float n0111 = dot(g0111, float4(Pf0.x, Pf1.y, Pf1.z, Pf1.w));
	float n1111 = dot(g1111, Pf1);

	float4 fade_xyzw = fade(Pf0);
	float4 n_0w = lerp(float4(n0000, n1000, n0100, n1100), float4(n0001, n1001, n0101, n1101), fade_xyzw.w);
	float4 n_1w = lerp(float4(n0010, n1010, n0110, n1110), float4(n0011, n1011, n0111, n1111), fade_xyzw.w);
	float4 n_zw = lerp(n_0w, n_1w, fade_xyzw.z);
	float2 n_yzw = lerp(float2(n_zw.x, n_zw.y), float2(n_zw.z, n_zw.w), fade_xyzw.y);
	float n_xyzw = lerp(n_yzw.x, n_yzw.y, fade_xyzw.x);
	return float(2.2) * n_xyzw;
}

float remap(float originalValue, float originalMin, float originalMax, float newMin, float newMax)
{
	return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
}

// ======================================================================

float worleyNoise3D(float3 p, float cellCount)
{
	return cells(p, cellCount);
}

float perlinNoise3D(float3 pIn, float frequency, int octaveCount)
{
	float octaveFrenquencyFactor = 2.0;			// noise frequency factor between octave, forced to 2

	// Compute the sum for each octave
	float sum = 0.0f;
	float weightSum = 0.0f;
	float weight = 0.5f;
	for (int oct = 0; oct < octaveCount; oct++)
	{
		// Perlin float3 is bugged in GLM on the Z axis :(, black stripes are visible
		// So instead we use 4d Perlin and only use xyz...
		//glm::float3 p(x * freq, y * freq, z * freq);
		//float val = glm::perlin(p, glm::float3(freq)) *0.5 + 0.5;

		float4 p = float4(pIn.x, pIn.y, pIn.z, 0.0) * frequency;
		float val = glmPerlin4D(p, frequency);

		sum += val * weight;
		weightSum += weight;

		weight *= weight;
		frequency *= octaveFrenquencyFactor;
	}

	float noise = (sum / weightSum);// *0.5 + 0.5;
	noise = min(noise, 1.0f);
	noise = max(noise, 0.0f);
	return noise;
}

float4 stackable3DNoise(uint3 pixel)
{
	float3 coord = float3(float(pixel.x) / 128.0, float(pixel.y) / 128.0, float(pixel.z) / 128.0);

	// Perlin FBM noise
	int octaveCount = 3;
	float frequency = 8.0;
	float perlinNoise = perlinNoise3D(coord, frequency, octaveCount);

	float PerlinWorleyNoise = 0.0f;
	{
		float cellCount = 4.0;
		float worleyNoise0 = (1.0 - worleyNoise3D(coord, cellCount * frequenceMul[0]));
		float worleyNoise1 = (1.0 - worleyNoise3D(coord, cellCount * frequenceMul[1]));
		float worleyNoise2 = (1.0 - worleyNoise3D(coord, cellCount * frequenceMul[2]));
		float worleyNoise3 = (1.0 - worleyNoise3D(coord, cellCount * frequenceMul[3]));
		float worleyNoise4 = (1.0 - worleyNoise3D(coord, cellCount * frequenceMul[4]));
		float worleyNoise5 = (1.0 - worleyNoise3D(coord, cellCount * frequenceMul[5]));	// half the frequency of texel, we should not go further (with cellCount = 32 and texture size = 64)

		// PerlinWorley noise as described p.101 of GPU Pro 7
		float worleyFBM = worleyNoise0 * 0.625f + worleyNoise1 * 0.25f + worleyNoise2 * 0.125f;

		PerlinWorleyNoise = remap(perlinNoise, 0.0, 1.0, worleyFBM, 1.0);
	}

	float cellCount = 4.0;
	float worleyNoise0 = (1.0 - worleyNoise3D(coord, cellCount * 1.0));
	float worleyNoise1 = (1.0 - worleyNoise3D(coord, cellCount * 2.0));
	float worleyNoise2 = (1.0 - worleyNoise3D(coord, cellCount * 4.0));
	float worleyNoise3 = (1.0 - worleyNoise3D(coord, cellCount * 8.0));
	float worleyNoise4 = (1.0 - worleyNoise3D(coord, cellCount * 16.0));
	//float worleyNoise5 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * 32));	
	//cellCount=2 -> half the frequency of texel, we should not go further (with cellCount = 32 and texture size = 64)

	// Three frequency of Worley FBM noise
	float worleyFBM0 = worleyNoise1 * 0.625f + worleyNoise2 * 0.25f + worleyNoise3 * 0.125f;
	float worleyFBM1 = worleyNoise2 * 0.625f + worleyNoise3 * 0.25f + worleyNoise4 * 0.125f;
	//float worleyFBM2 = worleyNoise3*0.625f + worleyNoise4*0.25f + worleyNoise5*0.125f;
	float worleyFBM2 = worleyNoise3 * 0.75f + worleyNoise4 * 0.25f;
	// cellCount=4 -> worleyNoise5 is just noise due to sampling frequency=texel frequency. So only take into account 2 frequencies for FBM

	return float4(PerlinWorleyNoise * PerlinWorleyNoise, worleyFBM0, worleyFBM1, worleyFBM2);
}

[numthreads(8, 8, 8)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	float4 perlin_worley = stackable3DNoise(DTid);
	PerlinWorleyTexture[DTid] = perlin_worley;
}