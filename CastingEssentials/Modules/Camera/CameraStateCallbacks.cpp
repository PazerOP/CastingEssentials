#include "Modules/Camera/CameraStateCallbacks.h"

#include <vector>

static std::vector<CameraStateCallbacks*> s_CallbackInstances;
class CameraStateCallbacksParent : public CameraStateCallbacks
{
public:
	CameraStateCallbacksParent() : CameraStateCallbacks(false) {}

	void SpecModeChanged(ObserverMode oldMode, ObserverMode& newMode) override
	{
		for (auto inst : s_CallbackInstances)
			inst->SpecModeChanged(oldMode, newMode);
	}
	void SpecTargetChanged(IClientEntity* oldEnt, IClientEntity*& newEnt) override
	{
		for (auto inst : s_CallbackInstances)
			inst->SpecTargetChanged(oldEnt, newEnt);
	}

	void SetupCameraTarget(ObserverMode mode, IClientEntity* target, CameraPtr& newCamera) override
	{
		for (auto inst : s_CallbackInstances)
			inst->SetupCameraTarget(mode, target, newCamera);
	}

	void SetupCameraSmooth(const CameraPtr& currentCamera, CameraPtr& targetCamera) override
	{
		for (auto inst : s_CallbackInstances)
			inst->SetupCameraSmooth(currentCamera, targetCamera);
	}

	bool GetFOVOverride(const CameraConstPtr& camera, float& fov) override
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

	void CameraCollapsed(const CameraPtr& previousCam, CameraPtr& newCam) override
	{
		for (auto inst : s_CallbackInstances)
			inst->CameraCollapsed(previousCam, newCam);
	}
};

CameraStateCallbacks::CameraStateCallbacks(bool nonParent)
{
	if (nonParent)
		s_CallbackInstances.push_back(this);
}
CameraStateCallbacks::~CameraStateCallbacks()
{
	if (auto found = std::find(s_CallbackInstances.begin(), s_CallbackInstances.end(), this); found != s_CallbackInstances.end())
		s_CallbackInstances.erase(found);
}

CameraStateCallbacks& CameraStateCallbacks::GetCallbacksParent()
{
	static CameraStateCallbacksParent s_Parent;
	return s_Parent;
}
