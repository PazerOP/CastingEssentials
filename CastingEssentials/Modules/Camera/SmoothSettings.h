#pragma once

#include <functional>

struct SmoothSettings
{
	bool m_TestFOV = true;
	bool m_TestDist = true;
	bool m_TestCooldown = true;
	bool m_TestLOS = true;

	float m_DurationOverride = -1;

	std::function<float(float x)> m_InterpolatorOverride;
};
