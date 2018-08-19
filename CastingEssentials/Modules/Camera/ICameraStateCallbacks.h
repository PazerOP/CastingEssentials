#pragma once

struct CameraStateDelta;
enum ObserverMode;

class ICameraStateCallbacks
{
public:
	virtual ~ICameraStateCallbacks() = default;

	virtual void OnCameraStateChanged(CameraStateDelta& delta) { }

	virtual bool GetFOVOverride(float& fov) { return false; }
};