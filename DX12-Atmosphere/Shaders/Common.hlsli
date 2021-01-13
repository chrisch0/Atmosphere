#ifndef __COMMON_HLSLI__
#define __COMMON_HLSLI__

uint ToComparableUint(float f)
{
	uint val = asuint(f);
	val ^= ((1 + ~(val >> 31)) | 0x80000000);
	return val;
}

float ComparableUintToFloat(uint ui)
{
	uint val = ui;
	val ^= (((val >> 31) - 1) | 0x80000000);
	return asfloat(val);
}

#endif