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