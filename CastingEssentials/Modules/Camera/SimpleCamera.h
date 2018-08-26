#pragma once
#include "Modules/Camera/ICamera.h"

class SimpleCamera : public ICamera
{
public:
	SimpleCamera()
	{
		m_Type = CameraType::Fixed;
	}
	SimpleCamera(const Vector& origin, const QAngle& angles, float fov, CameraType camType = CameraType::Fixed)
	{
		m_Origin = origin;
		m_Angles = angles;
		m_FOV = fov;
		m_Type = camType;
	}

	void Reset() override {}
	int GetAttachedEntity() const override { return 0; }
	const char* GetDebugName() const override { return "SimpleCamera"; }

	using ICamera::m_Origin;
	using ICamera::m_Angles;
	using ICamera::m_FOV;
	using ICamera::m_Type;

protected:
	bool IsCollapsible() const override { return false; }
	void Update(float dt, uint32_t frame) override {}
};
