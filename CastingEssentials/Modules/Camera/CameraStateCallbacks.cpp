#include "Modules/Camera/CameraStateCallbacks.h"

#include <vector>

static std::vector<CameraStateCallbacks*> s_CallbackInstances;

CameraStateCallbacks::CameraStateCallbacks()
{
	s_CallbackInstances.push_back(this);
}
CameraStateCallbacks::~CameraStateCallbacks()
{
	s_CallbackInstances.erase(std::find(s_CallbackInstances.begin(), s_CallbackInstances.end(), this));
}

void CameraStateCallbacks::RunSetupCameraState(CamStateData& state)
{
	for (auto inst : s_CallbackInstances)
		inst->SetupCameraState(state);
}

void CameraStateCallbacks::RunSetupCameraTarget(const CamStateData& state, CameraPtr& newCamera)
{
	for (auto inst : s_CallbackInstances)
		inst->SetupCameraTarget(state, newCamera);
}

void CameraStateCallbacks::RunSetupCameraSmooth(const CamStateData& state, const CameraPtr& currentCamera, CameraPtr& targetCamera)
{
	for (auto inst : s_CallbackInstances)
		inst->SetupCameraSmooth(state, currentCamera, targetCamera);
}

bool CameraStateCallbacks::RunGetFOVOverride(const CameraConstPtr& camera, float& fov)
{
	bool overridden = false;

	for (auto inst : s_CallbackInstances)
	{
		if (inst->GetFOVOverride(camera, fov))
		{
#ifdef DEBUG
			AssertMsg(!overridden, "Attempted to override fov from two places at once!");
			overridden = true;
#else
			return true;
#endif
		}
	}

	return overridden;
}