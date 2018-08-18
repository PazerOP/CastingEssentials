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
}

void SimpleCameraSmooth::Update(float dt)
{
	const auto startCam = GetStartCamera();
	const auto endCam = GetEndCamera();

	startCam->Update(dt);
	endCam->Update(dt);

	if (!IsSmoothComplete())
		m_CurrentTime = std::min(m_CurrentTime + dt, m_Duration);

	const float progress = GetProgress();

	m_Origin = Lerp(progress, startCam->GetOrigin(), endCam->GetOrigin());
	m_Angles = Lerp(progress, startCam->GetAngles(), endCam->GetAngles());
	m_FOV = Lerp(progress, startCam->GetFOV(), endCam->GetFOV());

	if (progress == 0)
		m_IsFirstPerson = startCam->IsFirstPerson();
	else if (progress == 1)
		m_IsFirstPerson = endCam->IsFirstPerson();
	else
		m_IsFirstPerson = false;
}
