Texture3D<float4> SrcMip : register(t0);

RWTexture3D<float4> OutMip1 : register(u0);
RWTexture3D<float4> OutMip2 : register(u1);
RWTexture3D<float4> OutMip3 : register(u2);

SamplerState BilinearClamp : register(s0);

cbuffer CB0 : register(b0)
{
	uint SrcMipLevel;
	uint NumMipLevels;
	uint2 pad;
	float3 TexelSize;
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

[numthreads(4, 4, 4)]
void main(uint3 globalID : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
	float3 uvw = TexelSize * (globalID.xyz + 0.5);
	float4 src1 = SrcMip.SampleLevel(BilinearClamp, uvw, SrcMipLevel);

	OutMip1[globalID.xyz] = PackColor(src1);

	if (NumMipLevels == 1)
		return;

	StoreColor(groupIndex, src1);

	GroupMemoryBarrierWithGroupSync();

	if ((groupIndex & 0x15) == 0)
	{
		float4 src2 = LoadColor(groupIndex + 0x01);
		float4 src3 = LoadColor(groupIndex + 0x04);
		float4 src4 = LoadColor(groupIndex + 0x05);
		float4 src5 = LoadColor(groupIndex + 0x10);
		float4 src6 = LoadColor(groupIndex + 0x11);
		float4 src7 = LoadColor(groupIndex + 0x14);
		float4 src8 = LoadColor(groupIndex + 0x15);
		src1 = 0.125 * (src1 + src2 + src3 + src4 + src5 + src6 + src7 + src8);

		OutMip2[globalID / 2] = PackColor(src1);
		StoreColor(groupIndex, src1);
	}

	if (NumMipLevels == 2)
		return;

	GroupMemoryBarrierWithGroupSync();

	if ((groupIndex & 0x3F) == 0)
	{
		float4 src2 = LoadColor(groupIndex + 0x02);
		float4 src3 = LoadColor(groupIndex + 0x08);
		float4 src4 = LoadColor(groupIndex + 0x0A);
		float4 src5 = LoadColor(groupIndex + 0x20);
		float4 src6 = LoadColor(groupIndex + 0x22);
		float4 src7 = LoadColor(groupIndex + 0x28);
		float4 src8 = LoadColor(groupIndex + 0x2A);
		src1 = 0.125 * (src1 + src2 + src3 + src4 + src5 + src6 + src7 + src8);
		OutMip3[globalID / 4] = PackColor(src1);
	}
}