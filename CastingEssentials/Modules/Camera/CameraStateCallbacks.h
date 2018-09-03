#pragma once

#include "Modules/Camera/ICamera.h"

class IClientEntity;
enum ObserverMode;
struct SmoothSettings;

class CameraStateCallbacks
{
public:
	CameraStateCallbacks(bool addToList = true);
	virtual ~CameraStateCallbacks();

	// Lets modules prevent camera mode/target from being changed at all
	virtual void SpecModeChanged(ObserverMode oldMode, ObserverMode& newMode) {}
	virtual void SpecTargetChanged(IClientEntity* oldEnt, IClientEntity*& newEnt) {}

	// 1st function to be called, lets modules override the new target camera
	virtual void SetupCameraTarget(ObserverMode mode, IClientEntity* target, CameraPtr& newCamera) {}

	// 2nd function to be called, after all other modules have had a chance to mess
	// with the target camera via SetupCameraTarget. As the name implies, used by CameraSmooths
	// module to create a camera smooth once we know for sure what our target will be.
	virtual void SetupCameraSmooth(const CameraPtr& currentCamera, CameraPtr& targetCamera, const SmoothSettings& settings) {}

	virtual bool GetFOVOverride(const CameraConstPtr& camera, float& fov) { return false; }

	// Called whenever the top-level camera (the one returned by CameraState::GetActiveCamera())
	// is collapsed.
	virtual void CameraCollapsed(const CameraPtr& previousCam, CameraPtr& newCam) {}

private:
	friend class CameraState;
	static CameraStateCallbacks& GetCallbacksParent();
};
