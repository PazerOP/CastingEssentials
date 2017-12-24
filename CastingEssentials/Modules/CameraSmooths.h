#pragma once

#include "PluginBase/ICameraOverride.h"
#include "PluginBase/Modules.h"

#include <mathlib/vector.h>
#include <ehandle.h>

#include <vector>

class ConVar;
class IConVar;
class C_BaseEntity;

class CameraSmooths : public Module, public ICameraOverride
{
public:
	CameraSmooths();

	static CameraSmooths* GetModule() { return Modules().GetModule<CameraSmooths>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<CameraSmooths>().c_str(); }

	static bool CheckDependencies();

	bool IsSmoothing() const { return m_InProgress; }
private:
	int m_EndMode;
	int m_EndTarget;
	bool m_InProgress;
	float m_LastHostTime;
	float m_StartDist;

	float m_LastAngPercentage;
	float m_LastOverallProgress;
	
	Vector m_SmoothStartPos;
	QAngle m_SmoothStartAng;
	float m_SmoothStartTime;

	bool InToolModeOverride() const override;
	bool IsThirdPersonCameraOverride() const override;
	bool SetupEngineViewOverride(Vector &origin, QAngle &angles, float &fov) override;
	
	ConVar* ce_smoothing_enabled;
	ConVar* ce_smoothing_fov;
	ConVar* ce_smoothing_force_distance;
	ConVar* ce_smoothing_max_distance;

	ConVar* ce_smoothing_linear_speed;
	ConVar* ce_smoothing_bezier_dist;
	ConVar* ce_smoothing_bezier_duration;

	ConVar* ce_smoothing_ang_bias;

	ConVar* ce_smoothing_mode;

	ConVar* ce_smoothing_debug;
	ConVar* ce_smoothing_debug_los;

	ConVar* ce_smoothing_check_los;
	ConVar* ce_smoothing_los_buffer;
	ConVar* ce_smoothing_los_min;

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

	void OnTick(bool inGame) override;

	static constexpr Color DBGMSG_COLOR = Color(255, 205, 68, 255);
};