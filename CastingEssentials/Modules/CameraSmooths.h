#pragma once

#include "Modules/Camera/CameraStateCallbacks.h"
#include "Modules/Camera/ICameraSmooth.h"
#include "PluginBase/Modules.h"

#include <convar.h>
#include <ehandle.h>
#include <mathlib/vector.h>

#include <vector>

class ConVar;
class IConVar;
class C_BaseEntity;

class CameraSmooths final : public Module<CameraSmooths>, CameraStateCallbacks
{
public:
	CameraSmooths();

	static bool CheckDependencies();
	static constexpr __forceinline const char* GetModuleName() { return "Camera Smooths"; }

protected:
	void LevelInit() override;

private:
	ConVar ce_smoothing_enabled;
	ConVar ce_smoothing_fov;
	ConVar ce_smoothing_force_distance;
	ConVar ce_smoothing_max_distance;

	ConVar ce_smoothing_rate;
	ConVar ce_smoothing_rate_dist_exp;

	ConVar ce_smoothing_debug;
	ConVar ce_smoothing_debug_los;

	ConVar ce_smoothing_los_buffer;
	ConVar ce_smoothing_los_min;

	ConVar ce_smoothing_cooldown;
	float m_LastSmoothEnd;

	void SetupCameraSmooth(const CameraPtr& currentCamera, CameraPtr& targetCamera, const SmoothSettings& settings) override;

	float TestVisibility(const Vector& eyePos, const Vector& targetPos) const;

	static constexpr Color DBGMSG_COLOR = Color(255, 205, 68, 255);
};
