#pragma once

#include "PluginBase/Modules.h"

#include <mathlib/vector.h>

class ConVar;
class IConVar;

class CameraSmooths : public Module
{
public:
	CameraSmooths();

	static CameraSmooths* GetModule() { return Modules().GetModule<CameraSmooths>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<CameraSmooths>().c_str(); }

	static bool CheckDependencies();
private:
	int inToolModeHook;
	int isThirdPersonCameraHook;
	int setupEngineViewHook;
	bool smoothEnding;
	int smoothEndMode;
	int smoothEndTarget;
	bool smoothInProgress;
	QAngle smoothLastAngles;
	Vector smoothLastOrigin;
	float smoothLastTime;

	bool InToolModeOverride();
	bool IsThirdPersonCameraOverride();
	bool SetupEngineViewOverride(Vector &origin, QAngle &angles, float &fov);

	class HLTVCameraOverride;

	ConVar *enabled;
	ConVar *max_angle_difference;
	ConVar *max_distance;
	ConVar *move_speed;
	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);
};