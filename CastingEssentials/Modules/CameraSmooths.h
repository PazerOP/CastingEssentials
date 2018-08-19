#pragma once

#include "Modules/Camera/ICameraSmooth.h"
#include "PluginBase/Modules.h"

#include <convar.h>
#include <ehandle.h>
#include <mathlib/vector.h>

#include <vector>

class ConVar;
class IConVar;
class C_BaseEntity;

class CameraSmooths : public Module<CameraSmooths>
{
public:
	CameraSmooths();

	static bool CheckDependencies();
	static constexpr __forceinline const char* GetModuleName() { return "Camera Smooths"; }

	std::shared_ptr<ICamera> CreatePlayerSmooth(const std::shared_ptr<ICamera>& currentCam, const std::shared_ptr<ICamera>& newCam) const;

private:
	ConVar ce_smoothing_enabled;
	ConVar ce_smoothing_fov;
	ConVar ce_smoothing_force_distance;
	ConVar ce_smoothing_max_distance;

	ConVar ce_smoothing_linear_speed;
	ConVar ce_smoothing_bezier_dist;
	ConVar ce_smoothing_bezier_duration;

	ConVar ce_smoothing_ang_bias;

	ConVar ce_smoothing_mode;

	ConVar ce_smoothing_debug;
	ConVar ce_smoothing_debug_los;

	ConVar ce_smoothing_check_los;
	ConVar ce_smoothing_los_buffer;
	ConVar ce_smoothing_los_min;

	ConCommand ce_smoothing_lerpto;
	void LerpTo(const CCommand& cmd) const;

	float TestVisibility(const Vector& eyePos, const Vector& targetPos) const;

	static constexpr Color DBGMSG_COLOR = Color(255, 205, 68, 255);
};