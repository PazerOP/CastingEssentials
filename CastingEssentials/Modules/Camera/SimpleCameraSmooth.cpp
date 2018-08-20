#include "Modules/Camera/SimpleCameraSmooth.h"

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
}

void SimpleCameraSmooth::Update(float dt, uint32_t frame)
{
	m_StartCamera->TryUpdate(dt, frame);
	TryCollapse(m_StartCamera);
	m_EndCamera->TryUpdate(dt, frame);
	TryCollapse(m_EndCamera);

	if (!IsSmoothComplete())
		m_CurrentTime = std::min(m_CurrentTime + dt, m_Duration);

	const float progress = m_Interpolator(GetProgress());

	m_Origin = Lerp(progress, m_StartCamera->GetOrigin(), m_EndCamera->GetOrigin());
	m_Angles = Lerp(progress, m_StartCamera->GetAngles(), m_EndCamera->GetAngles());
	m_FOV = Lerp(progress, m_StartCamera->GetFOV(), m_EndCamera->GetFOV());

	if (progress == 0)
		m_IsFirstPerson = m_StartCamera->IsFirstPerson();
	else if (progress == 1)
		m_IsFirstPerson = m_EndCamera->IsFirstPerson();
	else
		m_IsFirstPerson = false;

	Assert(m_Origin.IsValid());
	Assert(m_Angles.IsValid());
	Assert(std::isfinite(m_FOV));
}
