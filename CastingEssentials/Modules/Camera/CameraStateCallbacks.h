#pragma once

#include "Modules/Camera/ICamera.h"

class C_BaseEntity;
enum ObserverMode;

struct CamStateData
{
	ObserverMode m_Mode;
	C_BaseEntity* m_PrimaryTarget;
};

class CameraStateCallbacks
{
public:
	CameraStateCallbacks();
	virtual ~CameraStateCallbacks();

	// 1st function to be called, lets modules prevent camera mode/target from being changed at all
	virtual void SetupCameraState(CamStateData& state) {}

	// 2nd function to be called, lets modules override the new target camera
	virtual void SetupCameraTarget(const CamStateData& state, CameraPtr& newCamera) {}

	// 3rd function to be called, after all other modules have had a chance to mess
	// with the target camera via SetupCameraTarget. As the name implies, used by CameraSmooths
	// module to create a camera smooth once we know for sure what our target will be.
	virtual void SetupCameraSmooth(const CamStateData& state, const CameraPtr& currentCamera, CameraPtr& targetCamera) {}

	virtual bool GetFOVOverride(const CameraConstPtr& camera, float& fov) { return false; }

	static void RunSetupCameraState(CamStateData& state);
	static void RunSetupCameraTarget(const CamStateData& state, CameraPtr& newCamera);
	static void RunSetupCameraSmooth(const CamStateData& state, const CameraPtr& currentCamera, CameraPtr& targetCamera);
	static bool RunGetFOVOverride(const CameraConstPtr& camera, float& fov);
};