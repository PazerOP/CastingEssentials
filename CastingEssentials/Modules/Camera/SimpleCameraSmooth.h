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

	void Reset() override;
	const char* GetDebugName() const override { return "SimpleCameraSmooth"; }

	float GetProgress() const override { return m_CurrentTime / m_Duration; }

	std::function<float(float x)> m_Interpolator = Interpolators::Linear;

	bool m_UpdateStartAngles = true;
	bool m_UpdateStartOrigin = true;
	bool m_UpdateStartFOV = true;

protected:
	bool IsCollapsible() const override { return true; }
	void Update(float dt, uint32_t frame) override;

private:
	CameraPtr m_StartCamera;
	CameraPtr m_EndCamera;
	float m_Duration;
	float m_CurrentTime = 0;

	bool m_RotatingClockwise;
	AngularImpulse m_AnglesVelocity;
	static void FlipYaw(Quaternion& q);
	static Quaternion QuaternionDelta(const Quaternion& a, const Quaternion& b);
	static Quaternion HackQuaternionScale(const Quaternion& a, float scalar);
	static Quaternion HackQuaternionAdd(const Quaternion& a, const Quaternion& b);
	static Quaternion HackQuaternionSlerp(const Quaternion& a, Quaternion b, float t);

	static float QuaternionAngleDist(const Quaternion& a, const Quaternion& b);

	Vector m_StartOrigin;
	QAngle m_StartAngles;
	float m_StartFOV;

	QAngle m_EndAngles;
};
