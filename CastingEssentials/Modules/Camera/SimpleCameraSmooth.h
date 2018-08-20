#pragma once

#include "Misc/Interpolators.h"
#include "Modules/Camera/ICamera.h"
#include "Modules/Camera/ICameraSmooth.h"

#include <functional>

class SimpleCameraSmooth final : public ICameraSmooth
{
public:
	SimpleCameraSmooth(CameraPtr&& startCamera, CameraPtr&& endCamera, float duration);

	using ICameraSmooth::GetStartCamera;
	using ICameraSmooth::GetEndCamera;

	CameraConstPtr GetStartCamera() const override { return m_StartCamera; }
	CameraConstPtr GetEndCamera() const override { return m_EndCamera; }

	void Reset() override { m_CurrentTime = 0; }
	const char* GetDebugName() const override { return "SimpleCameraSmooth"; }

	float GetProgress() const override { return m_CurrentTime / m_Duration; }

	std::function<float(float x)> m_Interpolator = Interpolators::Linear;

protected:
	bool IsCollapsible() const override { return true; }
	void Update(float dt, uint32_t frame) override;

private:
	CameraPtr m_StartCamera;
	CameraPtr m_EndCamera;
	float m_Duration;
	float m_CurrentTime = 0;
};