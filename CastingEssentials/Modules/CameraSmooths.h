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

	Vector m_SmoothBeginPos;

	Vector m_SmoothStartPos;
	QAngle m_SmoothStartAng;
	float m_SmoothStartTime;

	Vector m_LastFramePos;
	QAngle m_LastFrameAng;

	bool InToolModeOverride();
	bool IsThirdPersonCameraOverride();
	bool SetupEngineViewOverride(Vector &origin, QAngle &angles, float &fov);

	class HLTVCameraOverride;

	ConVar *enabled;
	ConVar *max_angle_difference;
	ConVar *max_distance;
	ConVar *max_speed;
	ConVar* ce_camerasmooths_duration;

	ConVar* ce_camerasmooths_pos_bias;
	ConVar* ce_camerasmooths_ang_bias;

	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);

	void OnTick(bool inGame) override;
};