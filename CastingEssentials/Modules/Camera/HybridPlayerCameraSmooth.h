#pragma once
#include "Modules/Camera/ICamera.h"
#include "Modules/Camera/ICameraSmooth.h"
#include "Modules/Camera/SimpleCamera.h"

class HybridPlayerCameraSmooth : public ICameraSmooth, public ICamera, public std::enable_shared_from_this<ICamera>
{
public:
	HybridPlayerCameraSmooth(CameraPtr&& startCamera, CameraPtr&& endCamera);

	using ICameraSmooth::GetStartCamera;
	using ICameraSmooth::GetEndCamera;

	CameraConstPtr GetStartCamera() const override { return m_StartCamera; }
	CameraConstPtr GetEndCamera() const override { return m_EndCamera; }
	CameraConstPtr GetSmoothedCamera() const override { return shared_from_this(); }

	void Update(float dt) override;
	float GetProgress() const override { return m_Percent; }
	void Reset() override;

	float m_LinearSpeed = 875;
	float m_BezierDist = 1000;
	float m_BezierDuration = 0.65;
	float m_AngleBias = 0.85;

	enum class SmoothingMode
	{
		Approach,
		Lerp,
	} m_SmoothingMode = SmoothingMode::Approach;

private:
	static float ComputeSmooth(float time, float startTime, float totalDist, float maxSpeed, float bezierDist, float bezierDuration, float& percent);

	CameraPtr m_StartCamera;
	CameraPtr m_EndCamera;

	float m_ElapsedTime = 0;
	float m_StartDist;
	float m_Percent = 0;
	float m_LastAngPercent = 0;
};