#pragma once
#include "Modules/Camera/ICamera.h"

class OrbitCamera : public ICamera
{
public:
	OrbitCamera();

	void Reset() override;
	void Update(float dt) override;
	int GetAttachedEntity() const override { return 0; }

	const char* GetDebugName() const override { return "OrbitCamera"; }

	Vector m_OrbitPoint;
	float m_Radius;
	float m_ZOffset;

	float m_OrbitsPerSecond = 1.0f / 60;

protected:
	bool IsCollapsible() const override { return false; }

private:
	float m_ElapsedTime = 0;
};