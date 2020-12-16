#pragma once
#include "stdafx.h"
#include "Math/VectorMath.h"
#include "Math/Common.h"

struct BoundingBox
{
	BoundingBox()
		: max(-FLT_MAX), min(FLT_MAX)
	{

	}
	void Encapsulate(const Vector3& point)
	{
		max = Max(max, point);
		min = Min(min, point);
	}

	void Union(const BoundingBox& box)
	{
		max = Max(max, box.max);
		min = Min(min, box.min);
	}
	Vector3 max;
	Vector3 min;
};