#include <debugoverlay_shared.h>

void NDebugOverlay::Cross3D(const Vector &position, float size, int r, int g, int b, bool noDepthTest, float flDuration)
{
	Line(position + Vector(size, 0, 0), position - Vector(size, 0, 0), r, g, b, noDepthTest, flDuration);
	Line(position + Vector(0, size, 0), position - Vector(0, size, 0), r, g, b, noDepthTest, flDuration);
	Line(position + Vector(0, 0, size), position - Vector(0, 0, size), r, g, b, noDepthTest, flDuration);
}

void NDebugOverlay::Line(const Vector &origin, const Vector &target, int r, int g, int b, bool noDepthTest, float duration)
{
	debugoverlay->AddLineOverlay(origin, target, r, g, b, noDepthTest, duration);
}

void NDebugOverlay::Box(const Vector &origin, const Vector &mins, const Vector &maxs, int r, int g, int b, int a, float flDuration)
{
	BoxAngles(origin, mins, maxs, vec3_angle, r, g, b, a, flDuration);
}

void NDebugOverlay::BoxAngles(const Vector &origin, const Vector &mins, const Vector &maxs, const QAngle &angles, int r, int g, int b, int a, float duration)
{
	debugoverlay->AddBoxOverlay(origin, mins, maxs, angles, r, g, b, a, duration);
}