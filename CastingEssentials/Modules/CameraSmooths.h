#pragma once

#include "PluginBase/Modules.h"

#include <mathlib/vector.h>
#include <ehandle.h>

#include <vector>

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
	float m_LastOverallProgress;
	float m_LastAngPercentage;

	bool InToolModeOverride();
	bool IsThirdPersonCameraOverride();
	bool SetupEngineViewOverride(Vector &origin, QAngle &angles, float &fov);

	class HLTVCameraOverride;

	ConVar *enabled;
	ConVar *max_angle;
	ConVar* ce_camerasmooths_min_distance;
	ConVar *max_distance;
	ConVar *max_speed;
	ConVar* ce_camerasmooths_duration;

	ConVar* ce_camerasmooths_pos_bias;
	ConVar* ce_camerasmooths_ang_bias;

	ConVar* ce_camerasmooths_debug;
	ConVar* ce_camerasmooths_debug_los;

	ConVar* ce_camerasmooths_check_los;
	ConVar* ce_camerasmooths_los_buffer;
	
	ConVar* ce_camerasmooths_avoid_scoped_snipers;

	struct CollisionTest
	{
		Vector m_Mins;
		Vector m_Maxs;

		CHandle<C_BaseEntity> m_Entity;
		float m_Visibility;
	};

	int m_CollisionTestFrame;
	std::vector<CollisionTest> m_CollisionTests;
	void UpdateCollisionTests();
	void DrawCollisionTests();
	float GetVisibility(int entIndex);

	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);

	void OnTick(bool inGame) override;

	static constexpr Color DBGMSG_COLOR = Color(255, 205, 68, 255);
};