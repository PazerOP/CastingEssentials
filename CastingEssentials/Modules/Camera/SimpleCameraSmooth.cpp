#include "Misc/Extras/QAngle.h"
#include "Misc/Extras/Quaternion.h"
#include "Modules/Camera/SimpleCameraSmooth.h"
#include "PluginBase/Interfaces.h"

#include <cdll_int.h>
#include <con_nprint.h>
#include <convar.h>

#include <algorithm>

#undef min

SimpleCameraSmooth::SimpleCameraSmooth(CameraPtr&& startCamera, CameraPtr&& endCamera, float duration) :
	m_StartCamera(std::move(startCamera)), m_EndCamera(std::move(endCamera)), m_Duration(duration)
{
	const auto startCam = GetStartCamera();
	m_Origin = startCam->GetOrigin();
	m_Angles = startCam->GetAngles();
	m_FOV = startCam->GetFOV();

	Assert(std::isfinite(m_Duration));
	Assert(m_Duration > 0);
}

void SimpleCameraSmooth::Reset()
{
	m_CurrentTime = 0;
}

void SimpleCameraSmooth::Update(float dt, uint32_t frame)
{
	m_StartCamera->TryUpdate(dt, frame);
	TryCollapse(m_StartCamera);
	m_EndCamera->TryUpdate(dt, frame);
	TryCollapse(m_EndCamera);

	const float previousProgressLinear = GetProgress();
	const float previousProgress = m_Interpolator(previousProgressLinear);

	if (!IsSmoothComplete())
	{
		if (m_CurrentTime <= 0)
		{
			m_StartAngles = m_Angles = AngleNormalize(m_StartCamera->GetAngles());
			m_StartOrigin = m_Origin = m_StartCamera->GetOrigin();
			m_StartFOV = m_FOV = m_StartCamera->GetFOV();

			const auto& endAngles = m_EndCamera->GetAngles();
			for (int i = 0; i < 3; i++)
				m_EndAngles[i] = m_StartAngles[i] + AngleDistance(endAngles[i], m_StartAngles[i]);
		}

		m_CurrentTime = std::min(m_CurrentTime + dt, m_Duration);
	}

	const float progressLinear = GetProgress();
	const float progress = m_Interpolator(progressLinear);
	float progressDelta = 1;
	if (previousProgress < 1)
		progressDelta = (progress - previousProgress) / (1 - previousProgress);

	if (m_UpdateStartOrigin)
		m_Origin = Lerp(progress, m_StartCamera->GetOrigin(), m_EndCamera->GetOrigin());
	else
		m_Origin = Lerp(progress, m_StartOrigin, m_EndCamera->GetOrigin());

	if (m_UpdateStartAngles)
	{
		const auto& newStartAngles = m_StartCamera->GetAngles();
		const auto& newEndAngles = m_EndCamera->GetAngles();

		// Just do linear interpolation between the start and end angles. We sum up the
		// euler angle differences so we can end up with start/end numbers greater/less
		// than 180.
		for (int i = 0; i < 3; i++)
		{
			m_StartAngles[i] += AngleDiff(newStartAngles[i], m_StartAngles[i]);
			m_EndAngles[i] += AngleDiff(newEndAngles[i], m_EndAngles[i]);
			m_Angles[i] = Lerp(progress, m_StartAngles[i], m_EndAngles[i]);
		}
	}
	else
		m_Angles = Lerp(progressDelta, m_Angles, m_EndCamera->GetAngles());

	if (m_UpdateStartFOV)
		m_FOV = Lerp(progress, m_StartCamera->GetFOV(), m_EndCamera->GetFOV());
	else
		m_FOV = Lerp(progress, m_StartFOV, m_EndCamera->GetFOV());

	if (progress == 0)
		m_Type = m_StartCamera->GetCameraType();
	else if (progress == 1)
		m_Type = m_EndCamera->GetCameraType();
	else
		m_Type = CameraType::Smooth;

	Assert(m_Origin.IsValid());
	Assert(m_Angles.IsValid());
	Assert(std::isfinite(m_FOV));
}
