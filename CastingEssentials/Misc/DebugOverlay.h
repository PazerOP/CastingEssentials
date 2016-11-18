#pragma once
#include <debugoverlay_shared.h>

namespace NDebugOverlay
{
	inline void TriangleIgnoreZ(const Vector& p1, const Vector& p2, const Vector& p3, int r, int g, int b, int a, bool noDepthTest, float duration)
	{
		NDebugOverlay::Triangle(p1, p2, p3, r, g, b, a, noDepthTest, duration);
		NDebugOverlay::Triangle(p1, p3, p2, r, g, b, a, noDepthTest, duration);
	}
}