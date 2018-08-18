#pragma once

#include "Modules/Camera/ICamera.h"
#include "Modules/Camera/ICameraSmooth.h"

class SimpleCameraSmooth final : public ICameraSmooth, public ICamera, public std::enable_shared_from_this<ICamera>
{
public:
	SimpleCameraSmooth(CameraPtr&& startCamera, CameraPtr&& endCamera, float duration);

	using ICameraSmooth::GetStartCamera;
	using ICameraSmooth::GetEndCamera;

	CameraConstPtr GetStartCamera() const override { return m_StartCamera; }
	CameraConstPtr GetEndCamera() const override { return m_EndCamera; }
	CameraConstPtr GetSmoothedCamera() const override { return shared_from_this(); }

	void Reset() override { m_CurrentTime = 0; }
	float GetProgress() const override { return m_CurrentTime / m_Duration; }

	void Update(float dt) override;

private:
	CameraPtr m_StartCamera;
	CameraPtr m_EndCamera;
	float m_Duration;
	float m_CurrentTime;
};