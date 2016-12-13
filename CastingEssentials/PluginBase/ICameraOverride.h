#pragma once

class ICameraOverride
{
public:
	virtual ~ICameraOverride() { }

	virtual bool InToolModeOverride() const = 0;
	virtual bool IsThirdPersonCameraOverride() const = 0;
	virtual bool SetupEngineViewOverride(Vector &origin, QAngle &angles, float &fov) = 0;
};