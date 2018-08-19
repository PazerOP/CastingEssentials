#pragma once
#include "Modules/Camera/ICamera.h"

class OrbitCamera : public ICamera
{
public:
	OrbitCamera();

	void Reset() override;
	void Update(float dt) override;

	Vector m_OrbitPoint;
	float m_Radius;
	float m_ZOffset;

	float m_OrbitsPerSecond = 1.0f / 60;

protected:
	bool IsCollapsible() const override { return false; }
	int GetAttachedEntity() const override { return 0; }

private:
	float m_ElapsedTime = 0;
};