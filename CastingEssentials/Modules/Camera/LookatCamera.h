#pragma once

#include "Modules/Camera/ICamera.h"

#include <shared/ehandle.h>

class IClientEntity;

class LookatCamera final : public ICamera
{
public:
	LookatCamera();

	void Reset() override;
	int GetAttachedEntity() const override { return 0; }
	const char* GetDebugName() const override { return "LookatCamera"; }
	bool IsCollapsible() const { return false; }  // Likely to continue moving, even after stopping

	using ICamera::m_Origin;
	using ICamera::m_FOV;

	float m_Inertia = 3;
	Vector m_TargetOffset = Vector(0, 0, 0);
	CHandle<IClientEntity> m_TargetEntity;

	static QAngle SmoothCameraAngle(const QAngle& currentAngle, const QAngle& targetAngle, float dt, float inertia);

protected:
	void Update(float dt, uint32_t frame) override;

private:
	QAngle GetTargetAngles() const;

	float m_ElapsedTime = 0;
};
