#include "Modules/Camera/OrbitCamera.h"

OrbitCamera::OrbitCamera()
{
	m_IsFirstPerson = false;
}

void OrbitCamera::Reset()
{
	m_ElapsedTime = 0;
}

void OrbitCamera::Update(float dt, uint32_t frame)
{
	m_ElapsedTime += dt;

	m_Origin.z = m_OrbitPoint.z + m_ZOffset;

	const float angle = m_ElapsedTime * m_OrbitsPerSecond * (2 * M_PI_F);
	m_Origin.x = m_OrbitPoint.x + std::cosf(angle) * m_Radius;
	m_Origin.y = m_OrbitPoint.y + std::sinf(angle) * m_Radius;

	// Angles
	VectorAngles(m_OrbitPoint - m_Origin, m_Angles);

	m_FOV = 90;
}
