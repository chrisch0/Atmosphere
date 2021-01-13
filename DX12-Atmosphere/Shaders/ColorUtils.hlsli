#ifndef __COLOR_UTILS_HLSII__
#define __COLOR_UTILS_HLSII__

// H[0, 360], S[0, 1], V[0, 1] to R[0, 1], G[0, 1], B[0, 1]
float3 HSVToRGB(float3 hsv)
{
	float c = hsv.z * hsv.y;
	float x = c * (1 - abs(fmod(hsv.x / 60.0f, 2.0f) - 1.0f));
	float m = hsv.z - c;
	float3 rgb = float3(0.0f, 0.0f, 0.0f);
	if (hsv.x < 60.0f)
	{
		rgb = float3(c, x, 0.0f);
	}
	else if (hsv.x < 120.0f)
	{
		rgb = float3(x, c, 0.0f);
	}
	else if (hsv.x < 180.0f)
	{
		rgb = float3(0.0f, c, x);
	}
	else if (hsv.x < 240.0f)
	{
		rgb = float3(0.0f, x, c);
	}
	else if (hsv.x < 300.0f)
	{
		rgb = float3(x, 0.0f, c);
	}
	else if (hsv.x < 360.0f)
	{
		rgb = float3(c, 0, x);
	}
	rgb += float3(m, m, m);
	return rgb;
}

//
// Gamma ramps and encoding transfer functions
//
// Orthogonal to color space though usually tightly coupled.  For instance, sRGB is both a
// color space (defined by three basis vectors and a white point) and a gamma ramp.  Gamma
// ramps are designed to reduce perceptual error when quantizing floats to integers with a
// limited number of bits.  More variation is needed in darker colors because our eyes are
// more sensitive in the dark.  The way the curve helps is that it spreads out dark values
// across more code words allowing for more variation.  Likewise, bright values are merged
// together into fewer code words allowing for less variation.
//
// The sRGB curve is not a true gamma ramp but rather a piecewise function comprising a linear
// section and a power function.  When sRGB-encoded colors are passed to an LCD monitor, they
// look correct on screen because the monitor expects the colors to be encoded with sRGB, and it
// removes the sRGB curve to linearize the values.  When textures are encoded with sRGB--as many
// are--the sRGB curve needs to be removed before involving the colors in linear mathematics such
// as physically based lighting.

float3 ApplySRGBCurve(float3 x)
{
	// Approximately pow(x, 1.0 / 2.2)
	return x < 0.0031308 ? 12.92 * x : 1.055 * pow(x, 1.0 / 2.4) - 0.055;
}

float3 RemoveSRGBCurve(float3 x)
{
	// Approximately pow(x, 2.2)
	return x < 0.04045 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

// These functions avoid pow() to efficiently approximate sRGB with an error < 0.4%.
float3 ApplySRGBCurve_Fast(float3 x)
{
	return x < 0.0031308 ? 12.92 * x : 1.13005 * sqrt(x - 0.00228) - 0.13448 * x + 0.005719;
}

float3 RemoveSRGBCurve_Fast(float3 x)
{
	return x < 0.04045 ? x / 12.92 : -7.43605 * x - 31.24297 * sqrt(-0.53792 * x + 1.279924) + 35.34864;
}

// The OETF recommended for content shown on HDTVs.  This "gamma ramp" may increase contrast as
// appropriate for viewing in a dark environment.  Always use this curve with Limited RGB as it is
// used in conjunction with HDTVs.
float3 ApplyREC709Curve(float3 x)
{
	return x < 0.0181 ? 4.5 * x : 1.0993 * pow(x, 0.45) - 0.0993;
}

float3 RemoveREC709Curve(float3 x)
{
	return x < 0.08145 ? x / 4.5 : pow((x + 0.0993) / 1.0993, 1.0 / 0.45);
}

// This is the new HDR transfer function, also called "PQ" for perceptual quantizer.  Note that REC2084
// does not also refer to a color space.  REC2084 is typically used with the REC2020 color space.
float3 ApplyREC2084Curve(float3 L)
{
	float m1 = 2610.0 / 4096.0 / 4;
	float m2 = 2523.0 / 4096.0 * 128;
	float c1 = 3424.0 / 4096.0;
	float c2 = 2413.0 / 4096.0 * 32;
	float c3 = 2392.0 / 4096.0 * 32;
	float3 Lp = pow(L, m1);
	return pow((c1 + c2 * Lp) / (1 + c3 * Lp), m2);
}

float3 RemoveREC2084Curve(float3 N)
{
	float m1 = 2610.0 / 4096.0 / 4;
	float m2 = 2523.0 / 4096.0 * 128;
	float c1 = 3424.0 / 4096.0;
	float c2 = 2413.0 / 4096.0 * 32;
	float c3 = 2392.0 / 4096.0 * 32;
	float3 Np = pow(N, 1 / m2);
	return pow(max(Np - c1, 0) / (c2 - c3 * Np), 1 / m1);
}

//
// Color space conversions
//
// These assume linear (not gamma-encoded) values.  A color space conversion is a change
// of basis (like in Linear Algebra).  Since a color space is defined by three vectors--
// the basis vectors--changing space involves a matrix-vector multiplication.  Note that
// changing the color space may result in colors that are "out of bounds" because some
// color spaces have larger gamuts than others.  When converting some colors from a wide
// gamut to small gamut, negative values may result, which are inexpressible in that new
// color space.
//
// It would be ideal to build a color pipeline which never throws away inexpressible (but
// perceivable) colors.  This means using a color space that is as wide as possible.  The
// XYZ color space is the neutral, all-encompassing color space, but it has the unfortunate
// property of having negative values (specifically in X and Z).  To correct this, a further
// transformation can be made to X and Z to make them always positive.  They can have their
// precision needs reduced by dividing by Y, allowing X and Z to be packed into two UNORM8s.
// This color space is called YUV for lack of a better name.
//

// Note:  Rec.709 and sRGB share the same color primaries and white point.  Their only difference
// is the transfer curve used.

float3 REC709toREC2020(float3 RGB709)
{
	static const float3x3 ConvMat =
	{
		0.627402, 0.329292, 0.043306,
		0.069095, 0.919544, 0.011360,
		0.016394, 0.088028, 0.895578
	};
	return mul(ConvMat, RGB709);
}

float3 REC2020toREC709(float3 RGB2020)
{
	static const float3x3 ConvMat =
	{
		1.660496, -0.587656, -0.072840,
		-0.124547, 1.132895, -0.008348,
		-0.018154, -0.100597, 1.118751
	};
	return mul(ConvMat, RGB2020);
}

float3 REC709toDCIP3(float3 RGB709)
{
	static const float3x3 ConvMat =
	{
		0.822458, 0.177542, 0.000000,
		0.033193, 0.966807, 0.000000,
		0.017085, 0.072410, 0.910505
	};
	return mul(ConvMat, RGB709);
}

float3 DCIP3toREC709(float3 RGB709)
{
	static const float3x3 ConvMat =
	{
		1.224947, -0.224947, 0.000000,
		-0.042056, 1.042056, 0.000000,
		-0.019641, -0.078651, 1.098291
	};
	return mul(ConvMat, RGB709);
}

// This assumes the default color gamut found in sRGB and REC709.  The color primaries determine these
// coefficients.  Note that this operates on linear values, not gamma space.
float RGBToLuminance(float3 x)
{
	return dot(x, float3(0.212671, 0.715160, 0.072169));        // Defined by sRGB/Rec.709 gamut
}


//
// Reinhard
// 

// The Reinhard tone operator.  Typically, the value of k is 1.0, but you can adjust exposure by 1/k.
// I.e. TM_Reinhard(x, 0.5) == TM_Reinhard(x * 2.0, 1.0)
float3 TM_Reinhard(float3 hdr, float k = 1.0)
{
	return hdr / (hdr + k);
}

// The inverse of Reinhard
float3 ITM_Reinhard(float3 sdr, float k = 1.0)
{
	return k * sdr / (k - sdr);
}

//
// Reinhard-Squared
//

// This has some nice properties that improve on basic Reinhard.  Firstly, it has a "toe"--that nice,
// parabolic upswing that enhances contrast and color saturation in darks.  Secondly, it has a long
// shoulder giving greater detail in highlights and taking longer to desaturate.  It's invertible, scales
// to HDR displays, and is easy to control.
//
// The default constant of 0.25 was chosen for two reasons.  It maps closely to the effect of Reinhard
// with a constant of 1.0.  And with a constant of 0.25, there is an inflection point at 0.25 where the
// curve touches the line y=x and then begins the shoulder.
//
// Note:  If you are currently using ACES and you pre-scale by 0.6, then k=0.30 looks nice as an alternative
// without any other adjustments.

float3 TM_ReinhardSq(float3 hdr, float k = 0.25)
{
	float3 reinhard = hdr / (hdr + k);
	return reinhard * reinhard;
}

float3 ITM_ReinhardSq(float3 sdr, float k = 0.25)
{
	return k * (sdr + sqrt(sdr)) / (1.0 - sdr);
}

//
// Stanard (New)
//

// This is the new tone operator.  It resembles ACES in many ways, but it is simpler to evaluate with ALU.  One
// advantage it has over Reinhard-Squared is that the shoulder goes to white more quickly and gives more overall
// brightness and contrast to the image.

float3 TM_Stanard(float3 hdr)
{
	return TM_Reinhard(hdr * sqrt(hdr), sqrt(4.0 / 27.0));
}

float3 ITM_Stanard(float3 sdr)
{
	return pow(ITM_Reinhard(sdr, sqrt(4.0 / 27.0)), 2.0 / 3.0);
}

// 8-bit should range from 16 to 235
float3 RGBFullToLimited8bit(float3 x)
{
	return saturate(x) * 219.0 / 255.0 + 16.0 / 255.0;
}

float3 RGBLimitedToFull8bit(float3 x)
{
	return saturate((x - 16.0 / 255.0) * 255.0 / 219.0);
}

// 10-bit should range from 64 to 940
float3 RGBFullToLimited10bit(float3 x)
{
	return saturate(x) * 876.0 / 1023.0 + 64.0 / 1023.0;
}

float3 RGBLimitedToFull10bit(float3 x)
{
	return saturate((x - 64.0 / 1023.0) * 1023.0 / 876.0);
}

#define COLOR_FORMAT_LINEAR            0
#define COLOR_FORMAT_sRGB_FULL        1
#define COLOR_FORMAT_sRGB_LIMITED    2
#define COLOR_FORMAT_Rec709_FULL    3
#define COLOR_FORMAT_Rec709_LIMITED    4
#define COLOR_FORMAT_HDR10            5
#define COLOR_FORMAT_TV_DEFAULT        COLOR_FORMAT_Rec709_LIMITED
#define COLOR_FORMAT_PC_DEFAULT        COLOR_FORMAT_sRGB_FULL

#define HDR_COLOR_FORMAT            COLOR_FORMAT_LINEAR
#define LDR_COLOR_FORMAT            COLOR_FORMAT_LINEAR
#define DISPLAY_PLANE_FORMAT        COLOR_FORMAT_PC_DEFAULT

float3 ApplyDisplayProfile(float3 x, int DisplayFormat)
{
	switch (DisplayFormat)
	{
	default:
	case COLOR_FORMAT_LINEAR:
		return x;
	case COLOR_FORMAT_sRGB_FULL:
		return ApplySRGBCurve(x);
	case COLOR_FORMAT_sRGB_LIMITED:
		return RGBFullToLimited10bit(ApplySRGBCurve(x));
	case COLOR_FORMAT_Rec709_FULL:
		return ApplyREC709Curve(x);
	case COLOR_FORMAT_Rec709_LIMITED:
		return RGBFullToLimited10bit(ApplyREC709Curve(x));
	case COLOR_FORMAT_HDR10:
		return ApplyREC2084Curve(REC709toREC2020(x));
	};
}

float3 RemoveDisplayProfile(float3 x, int DisplayFormat)
{
	switch (DisplayFormat)
	{
	default:
	case COLOR_FORMAT_LINEAR:
		return x;
	case COLOR_FORMAT_sRGB_FULL:
		return RemoveSRGBCurve(x);
	case COLOR_FORMAT_sRGB_LIMITED:
		return RemoveSRGBCurve(RGBLimitedToFull10bit(x));
	case COLOR_FORMAT_Rec709_FULL:
		return RemoveREC709Curve(x);
	case COLOR_FORMAT_Rec709_LIMITED:
		return RemoveREC709Curve(RGBLimitedToFull10bit(x));
	case COLOR_FORMAT_HDR10:
		return REC2020toREC709(RemoveREC2084Curve(x));
	};
}

float3 ConvertColor(float3 x, int FromFormat, int ToFormat)
{
	if (FromFormat == ToFormat)
		return x;

	return ApplyDisplayProfile(RemoveDisplayProfile(x, FromFormat), ToFormat);
}


#endif