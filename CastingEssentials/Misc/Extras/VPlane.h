#pragma once

#include <mathlib/vplane.h>

inline void VPlaneInit(VPlane& plane, const Vector& vNormal, const Vector& vPoint)
{
	plane.m_Normal = vNormal;
	plane.m_Dist = vNormal.Dot(vPoint);
}
inline VPlane VPlaneInit(const Vector& vNormal, const Vector& vPoint)
{
	VPlane retVal;
	VPlaneInit(retVal, vNormal, vPoint);
	return retVal;
}

inline void VPlaneInit(VPlane& plane, const Vector& p0, const Vector& p1, const Vector& p2)
{
	VPlaneInit(plane, (p1 - p0).Cross(p2 - p0).Normalized(), p0);
}
inline VPlane VPlaneInit(const Vector& p0, const Vector& p1, const Vector& p2)
{
	VPlane retVal;
	VPlaneInit(p0, p1, p2);
	return retVal;
}

// Returns true if the plane intersected with the line.
inline bool VPlaneIntersectLine(const VPlane& plane, const Vector& p0, const Vector& p1, Vector* intersection = nullptr, bool lineSegment = true)
{
	//if (plane.GetPointSideExact(p0) == plane.GetPointSideExact(p1))
	//	return false;	// Both points of the line are on the same side of the plane, no intersection

	const Vector lineDir = p1 - p0;
	const float lineLength = lineDir.Length();
	const Vector lineDirNorm = lineDir / lineLength;

	if (lineDirNorm.Dot(plane.m_Normal) == 0)
		return false;	// Line and plane are parallel, no intersection

	// Make sure we have something here
	if (!intersection)
		intersection = (Vector*)stackalloc(sizeof(Vector));

	const Vector& planePoint = plane.GetPointOnPlane();

	// Substitute the parametric equation of the line into the normal equation of the plane
	const float upper = (planePoint - p0).Dot(plane.m_Normal);
	const float lower = lineDirNorm.Dot(plane.m_Normal);
	const float d = upper / lower;

	if (!lineSegment || d >= 0 && d <= lineLength)
	{
		if (intersection)
			*intersection = d * lineDirNorm + p0;

		return true;    // Intersection
	}

	return false;       // No intersection
}