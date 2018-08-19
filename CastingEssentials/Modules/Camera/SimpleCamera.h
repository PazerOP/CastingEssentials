#pragma once
#include "Modules/Camera/ICamera.h"

class SimpleCamera : public ICamera
{
public:
	SimpleCamera()
	{
		m_IsFirstPerson = false;
	}
	SimpleCamera(const Vector& origin, const QAngle& angles, float fov, bool isFirstPerson)
	{
		m_Origin = origin;
		m_Angles = angles;
		m_FOV = fov;
		m_IsFirstPerson = isFirstPerson;
	}

	void Reset() override {}
	void Update(float dt) override {}
	int GetAttachedEntity() const override { return 0; }

	using ICamera::m_Origin;
	using ICamera::m_Angles;
	using ICamera::m_FOV;
	using ICamera::m_IsFirstPerson;

protected:
	bool IsCollapsible() const override { return false; }
};