#ifndef NON_POWER_OF_TWO
#define NON_POWER_OF_TWO 0
#endif

Texture2D<float4> SrcMip : register(t0);

RWTexture2D<float4> OutMip1 : register(u0);
RWTexture2D<float4> OutMip2 : register(u1);
RWTexture2D<float4> OutMip3 : register(u2);
RWTexture2D<float4> OutMip4 : register(u3);

SamplerState BilinearClamp : register(s0);

cbuffer CB0 : register(b0)
{
	uint SrcMipLevel;
	uint NumMipLevels;
	uint pad0;
	uint pad1;
	float2 TexelSize;
}

groupshared float gs_R[64];
groupshared float gs_G[64];
groupshared float gs_B[64];
groupshared float gs_A[64];

void StoreColor(uint index, float4 color)
{
	gs_R[index] = color.r;
	gs_G[index] = color.g;
	gs_B[index] = color.b;
	gs_A[index] = color.a;
}

float4 LoadColor(uint index)
{
	return float4(gs_R[index], gs_G[index], gs_B[index], gs_A[index]);
}

float3 ApplySRGBCurve(float3 x)
{
	// This is exactly the sRGB curve
	//return x < 0.0031308 ? 12.92 * x : 1.055 * pow(abs(x), 1.0 / 2.4) - 0.055;

	// This is cheaper but nearly equivalent
	return x < 0.0031308 ? 12.92 * x : 1.13005 * sqrt(abs(x - 0.00228)) - 0.13448 * x + 0.005719;
}

float4 PackColor(float4 Linear)
{
#ifdef CONVERT_TO_SRGB
	return float4(ApplySRGBCurve(Linear.rgb), Linear.a);
#else
	return Linear;
#endif
}

[numthreads(8, 8, 1)]
void main( uint3 globalID : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex )
{
#if NON_POWER_OF_TWO == 0
	float2 uv = TexelSize * (globalID.xy + 0.5);
	float4 src1 = SrcMip.SampleLevel(BilinearClamp, uv, SrcMipLevel);
#elif NON_POWER_OF_TWO == 1
	float2 uv1 = TexelSize * (globalID.xy + float2(0.25, 0.5));
	float2 off = TexelSize * float2(0.5, 0.0);
	float4 src1 = 0.5 * (SrcMip.SampleLevel(BilinearClamp, uv1, SrcMipLevel) + SrcMip.SampleLevel(BilinearClamp, uv1 + off, SrcMipLevel));
#elif NON_POWER_OF_TWO == 2
	float2 uv1 = TexelSize * (globalID.xy + float2(0.5, 0.25));
	float2 off = TexelSize * float2(0.0, 0.05);
	float4 src1 = 0.5 * (SrcMip.SampleLevel(BilinearClamp, uv1, SrcMipLevel) + SrcMip.SampleLevel(BilinearClamp, uv1 + off, SrcMipLevel));
#elif NON_POWER_OF_TWO == 3
	float2 uv1 = TexelSize * (globalID.xy + float2(0.25, 0.25));
	float2 o = TexelSize * 0.5;
	float4 src1 = SrcMip.SampleLevel(BilinearClamp, uv1, SrcMipLevel);
	src1 += SrcMip.SampleLevel(BilinearClamp, uv1 + float2(o.x, 0.0), SrcMipLevel);
	src1 += SrcMip.SampleLevel(BilinearClamp, uv1 + float2(0.0, o.y), SrcMipLevel);
	src1 += SrcMip.SampleLevel(BilinearClamp, uv1 + float2(o.x, o.y), SrcMipLevel);
	src1 *= 0.25;
#endif

	OutMip1[globalID.xy] = PackColor(src1);

	if (NumMipLevels == 1)
		return;

	StoreColor(groupIndex, src1);

	GroupMemoryBarrierWithGroupSync();

	if ((groupIndex & 0x9) == 0)
	{
		float4 src2 = LoadColor(groupIndex + 0x01);
		float4 src3 = LoadColor(groupIndex + 0x08);
		float4 src4 = LoadColor(groupIndex + 0x09);
		src1 = 0.25 * (src1 + src2 + src3 + src4);

		OutMip2[globalID.xy / 2] = PackColor(src1);
		StoreColor(groupIndex, src1);
	}

	if (NumMipLevels == 2)
		return;

	GroupMemoryBarrierWithGroupSync();

	if ((groupIndex & 0x1B) == 0)
	{
		float4 src2 = LoadColor(groupIndex + 0x02);
		float4 src3 = LoadColor(groupIndex + 0x10);
		float4 src4 = LoadColor(groupIndex + 0x12);
		src1 = 0.25 * (src1 + src2 + src3 + src4);

		OutMip3[globalID.xy / 4] = PackColor(src1);
		StoreColor(groupIndex, src1);
	}

	if (NumMipLevels == 3)
		return;

	GroupMemoryBarrierWithGroupSync();
	
	if (groupIndex == 0)
	{
		float4 src2 = LoadColor(groupIndex + 0x04);
		float4 src3 = LoadColor(groupIndex + 0x20);
		float4 src4 = LoadColor(groupIndex + 0.24);
		src1 = 0.25 * (src1 + src2 + src3 + src4);

		OutMip4[globalID.xy / 8] = PackColor(src1);
	}
}