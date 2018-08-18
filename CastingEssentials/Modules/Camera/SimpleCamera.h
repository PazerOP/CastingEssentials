#pragma once
#include "Modules/Camera/ICamera.h"

class SimpleCamera : public ICamera
{
public:
	SimpleCamera()
	{
		m_IsFirstPerson = false;
	}

	void Reset() {}
	void Update(float dt) {}

	using ICamera::m_Origin;
	using ICamera::m_Angles;
	using ICamera::m_FOV;
	using ICamera::m_IsFirstPerson;
};