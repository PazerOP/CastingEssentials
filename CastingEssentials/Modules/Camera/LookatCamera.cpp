#include "LookatCamera.h"

#include <icliententity.h>

#include <algorithm>

LookatCamera::LookatCamera()
{
	m_Type = CameraType::Fixed;
}

void LookatCamera::Reset()
{
	m_ElapsedTime = 0;
	m_Angles = GetTargetAngles();
}

QAngle LookatCamera::SmoothCameraAngle(const QAngle& currentAngle, const QAngle& targetAngle, float dt, float inertia)
{
	dt = std::clamp(dt * inertia, 0.01f, 1.0f);
	return Lerp(dt, currentAngle, targetAngle);
}

void LookatCamera::Update(float dt, uint32_t frame)
{
	auto targetAngles = GetTargetAngles();

	if (m_ElapsedTime == 0)
		m_Angles = targetAngles;
	else
		m_Angles = SmoothCameraAngle(m_Angles, targetAngles, dt, m_Inertia);

	m_ElapsedTime += dt;
}

QAngle LookatCamera::GetTargetAngles() const
{
	auto targetPos = m_TargetOffset;
	if (auto ent = m_TargetEntity.Get())
		targetPos += ent->GetAbsOrigin();

	QAngle targetAngles;
	VectorAngles(targetPos - m_Origin, targetAngles);
	return targetAngles;
}
