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

void NDebugOverlay::Text(const Vector& origin, const char* text, bool bViewCheck, float duration)
{
	debugoverlay->AddTextOverlay(origin, duration, "%s", text);
}

void NDebugOverlay::Triangle(const Vector& p1, const Vector& p2, const Vector& p3, int r, int g, int b, int a, bool noDepthTest, float duration)
{
	debugoverlay->AddTriangleOverlay(p1, p2, p3, r, g, b, a, noDepthTest, duration);
}

void NDebugOverlay::Cross3DOriented(const Vector& position, const QAngle& angles, float size, int r, int g, int b, bool noDepthTest, float flDuration)
{
	Vector forward, right, up;
	AngleVectors(angles, &forward, &right, &up);

	forward *= size;
	right *= size;
	up *= size;

	Line(position + right, position - right, r, g, b, noDepthTest, flDuration);
	Line(position + forward, position - forward, r, g, b, noDepthTest, flDuration);
	Line(position + up, position - up, r, g, b, noDepthTest, flDuration);
}

void NDebugOverlay::EntityText(int entityID, int text_offset, const char *text, float duration, int r, int g, int b, int a)
{
	debugoverlay->AddEntityTextOverlay(entityID, text_offset, duration,
		(int)clamp(r * 255.f, 0.f, 255.f), (int)clamp(g * 255.f, 0.f, 255.f), (int)clamp(b * 255.f, 0.f, 255.f),
		(int)clamp(a * 255.f, 0.f, 255.f), text);
}