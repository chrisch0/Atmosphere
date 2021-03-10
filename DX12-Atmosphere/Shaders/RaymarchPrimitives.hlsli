float dot2(in float2 v) { return dot(v, v); }
float dot2(in float3 v) { return dot(v, v); }
float ndot(in float2 a, in float2 b) { return a.x*b.x - a.y*b.y; }

float sdPlane(float3 p)
{
	return p.y;
}

float sdSphere(float3 p, float s)
{
	return length(p) - s;
}

float sdBox(float3 p, float3 b)
{
	float3 d = abs(p) - b;
	return min(max(d.x, max(d.y, d.z)), 0.0) + length(max(d, 0.0));
}

float sdBoundingBox(float3 p, float3 b, float e)
{
	p = abs(p) - b;
	float3 q = abs(p + e) - e;

	return min(min(
		length(max(float3(p.x, q.y, q.z), 0.0)) + min(max(p.x, max(q.y, q.z)), 0.0),
		length(max(float3(q.x, p.y, q.z), 0.0)) + min(max(q.x, max(p.y, q.z)), 0.0)),
		length(max(float3(q.x, q.y, p.z), 0.0)) + min(max(q.x, max(q.y, p.z)), 0.0));
}
float sdEllipsoid(in float3 p, in float3 r) // approximated
{
	float k0 = length(p / r);
	float k1 = length(p / (r*r));
	return k0 * (k0 - 1.0) / k1;
}

float sdTorus(float3 p, float2 t)
{
	return length(float2(length(p.xz) - t.x, p.y)) - t.y;
}

float sdCappedTorus(in float3 p, in float2 sc, in float ra, in float rb)
{
	p.x = abs(p.x);
	float k = (sc.y*p.x > sc.x*p.y) ? dot(p.xy, sc) : length(p.xy);
	return sqrt(dot(p, p) + ra * ra - 2.0*ra*k) - rb;
}

float sdHexPrism(float3 p, float2 h)
{
	float3 q = abs(p);

	const float3 k = float3(-0.8660254, 0.5, 0.57735);
	p = abs(p);
	p.xy -= 2.0*min(dot(k.xy, p.xy), 0.0)*k.xy;
	float2 d = float2(
		length(p.xy - float2(clamp(p.x, -k.z*h.x, k.z*h.x), h.x))*sign(p.y - h.x),
		p.z - h.y);
	return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdOctogonPrism(in float3 p, in float r, float h)
{
	const float3 k = float3(-0.9238795325,   // sqrt(2+sqrt(2))/2 
		0.3826834323,   // sqrt(2-sqrt(2))/2
		0.4142135623); // sqrt(2)-1 
// reflections
	p = abs(p);
	p.xy -= 2.0*min(dot(float2(k.x, k.y), p.xy), 0.0)*float2(k.x, k.y);
	p.xy -= 2.0*min(dot(float2(-k.x, k.y), p.xy), 0.0)*float2(-k.x, k.y);
	// polygon side
	p.xy -= float2(clamp(p.x, -k.z*r, k.z*r), r);
	float2 d = float2(length(p.xy)*sign(p.y), p.z - h);
	return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdCapsule(float3 p, float3 a, float3 b, float r)
{
	float3 pa = p - a, ba = b - a;
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
	return length(pa - ba * h) - r;
}

float sdRoundCone(in float3 p, in float r1, float r2, float h)
{
	float2 q = float2(length(p.xz), p.y);

	float b = (r1 - r2) / h;
	float a = sqrt(1.0 - b * b);
	float k = dot(q, float2(-b, a));

	if (k < 0.0) return length(q) - r1;
	if (k > a*h) return length(q - float2(0.0, h)) - r2;

	return dot(q, float2(a, b)) - r1;
}

float sdRoundCone(float3 p, float3 a, float3 b, float r1, float r2)
{
	// sampling independent computations (only depend on shape)
	float3  ba = b - a;
	float l2 = dot(ba, ba);
	float rr = r1 - r2;
	float a2 = l2 - rr * rr;
	float il2 = 1.0 / l2;

	// sampling dependant computations
	float3 pa = p - a;
	float y = dot(pa, ba);
	float z = y - l2;
	float x2 = dot2(pa*l2 - ba * y);
	float y2 = y * y*l2;
	float z2 = z * z*l2;

	// single square root!
	float k = sign(rr)*rr*rr*x2;
	if (sign(z)*a2*z2 > k) return  sqrt(x2 + z2)        *il2 - r2;
	if (sign(y)*a2*y2 < k) return  sqrt(x2 + y2)        *il2 - r1;
	return (sqrt(x2*a2*il2) + y * rr)*il2 - r1;
}

float sdTriPrism(float3 p, float2 h)
{
	const float k = sqrt(3.0);
	h.x *= 0.5*k;
	p.xy /= h.x;
	p.x = abs(p.x) - 1.0;
	p.y = p.y + 1.0 / k;
	if (p.x + k * p.y > 0.0) p.xy = float2(p.x - k * p.y, -k * p.x - p.y) / 2.0;
	p.x -= clamp(p.x, -2.0, 0.0);
	float d1 = length(p.xy)*sign(-p.y)*h.x;
	float d2 = abs(p.z) - h.y;
	return length(max(float2(d1, d2), 0.0)) + min(max(d1, d2), 0.);
}

// vertical
float sdCylinder(float3 p, float2 h)
{
	float2 d = abs(float2(length(p.xz), p.y)) - h;
	return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

// arbitrary orientation
float sdCylinder(float3 p, float3 a, float3 b, float r)
{
	float3 pa = p - a;
	float3 ba = b - a;
	float baba = dot(ba, ba);
	float paba = dot(pa, ba);

	float x = length(pa*baba - ba * paba) - r * baba;
	float y = abs(paba - baba * 0.5) - baba * 0.5;
	float x2 = x * x;
	float y2 = y * y*baba;
	float d = (max(x, y) < 0.0) ? -min(x2, y2) : (((x > 0.0) ? x2 : 0.0) + ((y > 0.0) ? y2 : 0.0));
	return sign(d)*sqrt(abs(d)) / baba;
}

// vertical
float sdCone(in float3 p, in float2 c, float h)
{
	float2 q = h * float2(c.x, -c.y) / c.y;
	float2 w = float2(length(p.xz), p.y);

	float2 a = w - q * clamp(dot(w, q) / dot(q, q), 0.0, 1.0);
	float2 b = w - q * float2(clamp(w.x / q.x, 0.0, 1.0), 1.0);
	float k = sign(q.y);
	float d = min(dot(a, a), dot(b, b));
	float s = max(k*(w.x*q.y - w.y*q.x), k*(w.y - q.y));
	return sqrt(d)*sign(s);
}

float sdCappedCone(in float3 p, in float h, in float r1, in float r2)
{
	float2 q = float2(length(p.xz), p.y);

	float2 k1 = float2(r2, h);
	float2 k2 = float2(r2 - r1, 2.0*h);
	float2 ca = float2(q.x - min(q.x, (q.y < 0.0) ? r1 : r2), abs(q.y) - h);
	float2 cb = q - k1 + k2 * clamp(dot(k1 - q, k2) / dot2(k2), 0.0, 1.0);
	float s = (cb.x < 0.0 && ca.y < 0.0) ? -1.0 : 1.0;
	return s * sqrt(min(dot2(ca), dot2(cb)));
}

float sdCappedCone(float3 p, float3 a, float3 b, float ra, float rb)
{
	float rba = rb - ra;
	float baba = dot(b - a, b - a);
	float papa = dot(p - a, p - a);
	float paba = dot(p - a, b - a) / baba;

	float x = sqrt(papa - paba * paba*baba);

	float cax = max(0.0, x - ((paba < 0.5) ? ra : rb));
	float cay = abs(paba - 0.5) - 0.5;

	float k = rba * rba + baba;
	float f = clamp((rba*(x - ra) + paba * baba) / k, 0.0, 1.0);

	float cbx = x - ra - f * rba;
	float cby = paba - f;

	float s = (cbx < 0.0 && cay < 0.0) ? -1.0 : 1.0;

	return s * sqrt(min(cax*cax + cay * cay*baba,
		cbx*cbx + cby * cby*baba));
}

// c is the sin/cos of the desired cone angle
float sdSolidAngle(float3 pos, float2 c, float ra)
{
	float2 p = float2(length(pos.xz), pos.y);
	float l = length(p) - ra;
	float m = length(p - c * clamp(dot(p, c), 0.0, ra));
	return max(l, m*sign(c.y*p.x - c.x*p.y));
}

float sdOctahedron(float3 p, float s)
{
	p = abs(p);
	float m = p.x + p.y + p.z - s;

	// exact distance
#if 0
	float3 o = min(3.0*p - m, 0.0);
	o = max(6.0*p - m * 2.0 - o * 3.0 + (o.x + o.y + o.z), 0.0);
	return length(p - s * o / (o.x + o.y + o.z));
#endif

	// exact distance
#if 1
	float3 q;
	if (3.0*p.x < m) q = p.xyz;
	else if (3.0*p.y < m) q = p.yzx;
	else if (3.0*p.z < m) q = p.zxy;
	else return m * 0.57735027;
	float k = clamp(0.5*(q.z - q.y + s), 0.0, s);
	return length(float3(q.x, q.y - s + k, q.z - k));
#endif

	// bound, not exact
#if 0
	return m * 0.57735027;
#endif
}

float sdPyramid(in float3 p, in float h)
{
	float m2 = h * h + 0.25;

	// symmetry
	p.xz = abs(p.xz);
	p.xz = (p.z > p.x) ? p.zx : p.xz;
	p.xz -= 0.5;

	// project into face plane (2D)
	float3 q = float3(p.z, h*p.y - 0.5*p.x, h*p.x + 0.5*p.y);

	float s = max(-q.x, 0.0);
	float t = clamp((q.y - 0.5*p.z) / (m2 + 0.25), 0.0, 1.0);

	float a = m2 * (q.x + s)*(q.x + s) + q.y*q.y;
	float b = m2 * (q.x + 0.5*t)*(q.x + 0.5*t) + (q.y - m2 * t)*(q.y - m2 * t);

	float d2 = min(q.y, -q.x*m2 - q.y*0.5) > 0.0 ? 0.0 : min(a, b);

	// recover 3D and scale, and add sign
	return sqrt((d2 + q.z*q.z) / m2) * sign(max(q.z, -p.y));;
}

// la,lb=semi axis, h=height, ra=corner
float sdRhombus(float3 p, float la, float lb, float h, float ra)
{
	p = abs(p);
	float2 b = float2(la, lb);
	float f = clamp((ndot(b, b - 2.0*p.xz)) / dot(b, b), -1.0, 1.0);
	float2 q = float2(length(p.xz - 0.5*b*float2(1.0 - f, 1.0 + f))*sign(p.x*b.y + p.z*b.x - b.x*b.y) - ra, p.y - h);
	return min(max(q.x, q.y), 0.0) + length(max(q, 0.0));
}

//------------------------------------------------------------------

float2 opU(float2 d1, float2 d2)
{
	return (d1.x < d2.x) ? d1 : d2;
}

//------------------------------------------------------------------

float2 map(in float3 pos)
{
	float2 res = float2(1e10, 0.0);

	{
		res = opU(res, float2(sdSphere(pos - float3(-2.0, 0.25, 0.0), 0.25), 26.9));
	}

	// bounding box
	if (sdBox(pos - float3(0.0, 0.3, -1.0), float3(0.35, 0.3, 2.5)) < res.x)
	{
		// more primitives
		res = opU(res, float2(sdBoundingBox(pos - float3(0.0, 0.25, 0.0), float3(0.3, 0.25, 0.2), 0.025), 16.9));
		res = opU(res, float2(sdTorus((pos - float3(0.0, 0.30, 1.0)).xzy, float2(0.25, 0.05)), 25.0));
		res = opU(res, float2(sdCone(pos - float3(0.0, 0.45, -1.0), float2(0.6, 0.8), 0.45), 55.0));
		res = opU(res, float2(sdCappedCone(pos - float3(0.0, 0.25, -2.0), 0.25, 0.25, 0.1), 13.67));
		res = opU(res, float2(sdSolidAngle(pos - float3(0.0, 0.00, -3.0), float2(3, 4) / 5.0, 0.4), 49.13));
	}

	// bounding box
	if (sdBox(pos - float3(1.0, 0.3, -1.0), float3(0.35, 0.3, 2.5)) < res.x)
	{
		// more primitives
		res = opU(res, float2(sdCappedTorus((pos - float3(1.0, 0.30, 1.0))*float3(1, -1, 1), float2(0.866025, -0.5), 0.25, 0.05), 8.5));
		res = opU(res, float2(sdBox(pos - float3(1.0, 0.25, 0.0), float3(0.3, 0.25, 0.1)), 3.0));
		res = opU(res, float2(sdCapsule(pos - float3(1.0, 0.00, -1.0), float3(-0.1, 0.1, -0.1), float3(0.2, 0.4, 0.2), 0.1), 31.9));
		res = opU(res, float2(sdCylinder(pos - float3(1.0, 0.25, -2.0), float2(0.15, 0.25)), 8.0));
		res = opU(res, float2(sdHexPrism(pos - float3(1.0, 0.2, -3.0), float2(0.2, 0.05)), 18.4));
	}

	// bounding box
	if (sdBox(pos - float3(-1.0, 0.35, -1.0), float3(0.35, 0.35, 2.5)) < res.x)
	{
		// more primitives
		res = opU(res, float2(sdPyramid(pos - float3(-1.0, -0.6, -3.0), 1.0), 13.56));
		res = opU(res, float2(sdOctahedron(pos - float3(-1.0, 0.15, -2.0), 0.35), 23.56));
		res = opU(res, float2(sdTriPrism(pos - float3(-1.0, 0.15, -1.0), float2(0.3, 0.05)), 43.5));
		res = opU(res, float2(sdEllipsoid(pos - float3(-1.0, 0.25, 0.0), float3(0.2, 0.25, 0.05)), 43.17));
		res = opU(res, float2(sdRhombus((pos - float3(-1.0, 0.34, 1.0)).xzy, 0.15, 0.25, 0.04, 0.08), 17.0));
	}

	// bounding box
	if (sdBox(pos - float3(2.0, 0.3, -1.0), float3(0.35, 0.3, 2.5)) < res.x)
	{
		// more primitives
		res = opU(res, float2(sdOctogonPrism(pos - float3(2.0, 0.2, -3.0), 0.2, 0.05), 51.8));
		res = opU(res, float2(sdCylinder(pos - float3(2.0, 0.15, -2.0), float3(0.1, -0.1, 0.0), float3(-0.2, 0.35, 0.1), 0.08), 31.2));
		res = opU(res, float2(sdCappedCone(pos - float3(2.0, 0.10, -1.0), float3(0.1, 0.0, 0.0), float3(-0.2, 0.40, 0.1), 0.15, 0.05), 46.1));
		res = opU(res, float2(sdRoundCone(pos - float3(2.0, 0.15, 0.0), float3(0.1, 0.0, 0.0), float3(-0.1, 0.35, 0.1), 0.15, 0.05), 51.7));
		res = opU(res, float2(sdRoundCone(pos - float3(2.0, 0.20, 1.0), 0.2, 0.1, 0.3), 37.0));
	}

	return res;
}

// http://iquilezles.org/www/articles/boxfunctions/boxfunctions.htm
float2 iBox(in float3 ro, in float3 rd, in float3 rad)
{
	float3 m = 1.0 / rd;
	float3 n = m * ro;
	float3 k = abs(m)*rad;
	float3 t1 = -n - k;
	float3 t2 = -n + k;
	return float2(max(max(t1.x, t1.y), t1.z),
		min(min(t2.x, t2.y), t2.z));
}

float2 raycast(in float3 ro, in float3 rd)
{
	float2 res = float2(-1.0, -1.0);

	float tmin = 1.0;
	float tmax = 20.0;

	//// raytrace floor plane
	//float tp1 = (0.0 - ro.y) / rd.y;
	//if (tp1 > 0.0)
	//{
	//	tmax = min(tmax, tp1);
	//	res = float2(tp1, 1.0);
	//}
	//else return res;
	float3 p = ro - EarthCenter;
	float p_dot_v = dot(p, rd);
	float p_dot_p = dot(p, p);
	float ray_earth_center_squared_distance = p_dot_p - p_dot_v * p_dot_v;
	float distance_to_intersection = -p_dot_v - sqrt(EarthCenter.y * EarthCenter.y - ray_earth_center_squared_distance);
	if (distance_to_intersection > 0.0)
	{
		tmax = min(tmax, distance_to_intersection);
		res = float2(distance_to_intersection, 1.0);
	}

	// raymarch primitives   
	float2 tb = iBox(ro - float3(0.0, 0.4, -0.5), rd, float3(2.5, 0.41, 3.0));
	if (tb.x<tb.y && tb.y>0.0 && tb.x < tmax)
	{
		//return float2(tb.x,2.0);
		tmin = max(tb.x, tmin);
		tmax = min(tb.y, tmax);

		float t = tmin;
		for (int i = 0; i < 70 && t < tmax; i++)
		{
			float2 h = map(ro + rd * t);
			if (abs(h.x) < (0.0001*t))
			{
				res = float2(t, h.y);
				break;
			}
			t += h.x;
		}
	}

	return res;
}

// http://iquilezles.org/www/articles/rmshadows/rmshadows.htm
float calcSoftshadow(in float3 ro, in float3 rd, in float mint, in float tmax)
{
	// bounding volume
	float tp = (0.8 - ro.y) / rd.y; if (tp > 0.0) tmax = min(tmax, tp);

	float res = 1.0;
	float t = mint;
	for (int i = 0; i < 24; i++)
	{
		float h = map(ro + rd * t).x;
		float s = clamp(8.0*h / t, 0.0, 1.0);
		res = min(res, s*s*(3.0 - 2.0*s));
		t += clamp(h, 0.02, 0.2);
		if (res<0.004 || t>tmax) break;
	}
	return clamp(res, 0.0, 1.0);
}

// http://iquilezles.org/www/articles/normalsSDF/normalsSDF.htm
float3 calcNormal(in float3 pos)
{
	// inspired by tdhooper and klems - a way to prevent the compiler from inlining map() 4 times
	float3 n = float3(0.0, 0.0, 0.0);
	for (int i = 0; i < 4; i++)
	{
		float3 e = 0.5773*(2.0*float3((((i + 3) >> 1) & 1), ((i >> 1) & 1), (i & 1)) - 1.0);
		n += e * map(pos + 0.0005*e).x;
		//if( n.x+n.y+n.z>100.0 ) break;
	}
	return normalize(n);   
}

float calcAO(in float3 pos, in float3 nor)
{
	float occ = 0.0;
	float sca = 1.0;
	for (int i = 0; i < 5; i++)
	{
		float h = 0.01 + 0.12*float(i) / 4.0;
		float d = map(pos + h * nor).x;
		occ += (h - d)*sca;
		sca *= 0.95;
		if (occ > 0.35) break;
	}
	return clamp(1.0 - 3.0*occ, 0.0, 1.0) * (0.5 + 0.5*nor.y);
}


