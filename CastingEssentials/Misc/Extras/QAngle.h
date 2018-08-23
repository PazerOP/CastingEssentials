#pragma once

#include <mathlib/vector.h>

inline QAngle AngleNormalize(const QAngle& q1)
{
	return QAngle(AngleNormalize(q1.x), AngleNormalize(q1.y), AngleNormalize(q1.z));
}
inline QAngle AngleNormalizePositive(const QAngle& q1)
{
	return QAngle(AngleNormalizePositive(q1.x), AngleNormalizePositive(q1.y), AngleNormalizePositive(q1.z));
}
