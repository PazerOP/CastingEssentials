#pragma once

#include "Modules/Camera/ICamera.h"
#include "Modules/Camera/ICameraSmooth.h"

class SimpleCameraSmooth final : public ICameraSmooth
{
public:
	SimpleCameraSmooth(CameraPtr&& startCamera, CameraPtr&& endCamera, float duration);

	using ICameraSmooth::GetStartCamera;
	using ICameraSmooth::GetEndCamera;

	CameraConstPtr GetStartCamera() const override { return m_StartCamera; }
	CameraConstPtr GetEndCamera() const override { return m_EndCamera; }

	void Reset() override { m_CurrentTime = 0; }
	float GetProgress() const override { return m_CurrentTime / m_Duration; }

	void Update(float dt) override;

protected:
	bool IsCollapsible() const override { return true; }

private:
	CameraPtr m_StartCamera;
	CameraPtr m_EndCamera;
	float m_Duration;
	float m_CurrentTime;
};