#include "CameraSmooths.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"
#include "Misc/DebugOverlay.h"
#include "Misc/HLTVCameraHack.h"
#include "Modules/Camera/HybridPlayerCameraSmooth.h"
#include "Modules/Camera/OrbitCamera.h"
#include "Modules/Camera/SimpleCameraSmooth.h"
#include "Modules/CameraState.h"
#include "Modules/CameraTools.h"

#include <client/hltvcamera.h>
#include <shareddefs.h>
#include <toolframework/ienginetool.h>
#include <cdll_int.h>
#include <view_shared.h>
#include <icliententity.h>
#include <engine/IEngineTrace.h>
#include <util_shared.h>
#include <vprof.h>

#undef min
#undef max
#include <algorithm>

MODULE_REGISTER(CameraSmooths);

static constexpr char SMOOTH_RATE_HELP[] = "<smooth duration> = <initial distance>^<ce_smoothing_rate_dist_exp> / <ce_smoothing_rate>";

CameraSmooths::CameraSmooths() :
	ce_smoothing_enabled("ce_smoothing_enabled", "0", FCVAR_NONE, "Enables smoothing between spectator targets."),
	ce_smoothing_fov("ce_smoothing_fov", "45", FCVAR_NONE, "Only targets within this FOV will be smoothed to.", true, 0, true, 180),
	ce_smoothing_max_distance("ce_smoothing_max_distance", "2250", FCVAR_NONE, "max distance at which smoothing will be performed"),
	ce_smoothing_force_distance("ce_smoothing_force_distance", "128", FCVAR_NONE, "Always smooth if we're closer than this distance."),

	ce_smoothing_rate("ce_smoothing_rate", "5", FCVAR_NONE,
		"Adjusts the speed of smooths. <smooth duration> = <initial distance>^<ce_smoothing_rate_dist_exp> / <ce_smoothing_rate>"),
	ce_smoothing_rate_dist_exp("ce_smoothing_rate_dist_exp", "0.315", FCVAR_NONE,
		"Adjusts the ratio of smooth duration vs distance. <smooth duration> = <initial distance>^<ce_smoothing_rate_dist_exp> / <ce_smoothing_rate>"),

	ce_smoothing_debug("ce_smoothing_debug", "0", FCVAR_NONE, "Prints debugging info about camera smooths to the console."),
	ce_smoothing_debug_los("ce_smoothing_debug_los", "0", FCVAR_NONE, "Draw points associated with LOS checking for camera smooths."),

	ce_smoothing_los_buffer("ce_smoothing_los_buffer", "32", FCVAR_NONE, "Additional space to give ourselves so we can sorta see around corners."),
	ce_smoothing_los_min("ce_smoothing_los_min", "0", FCVAR_NONE, "Minimum percentage of points that must pass the LOS check before we allow ourselves to smooth to a target.", true, 0, true, 1),

	ce_smoothing_cooldown("ce_smoothing_cooldown", "5", FCVAR_NONE, "Minimum number of seconds that must pass between one camera smooth ending and another beginning.")
{
}

bool CameraSmooths::CheckDependencies()
{
	Modules().Depend<CameraState>();

	bool ready = true;

	if (!Interfaces::GetClientEngineTools())
	{
		PluginWarning("Required interface IClientEngineTools for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Interfaces::GetEngineClient())
	{
		PluginWarning("Required interface IVEngineClient for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Interfaces::GetEngineTool())
	{
		PluginWarning("Required interface IEngineTool for module %s not available!\n", GetModuleName());
		ready = false;
	}

	try
	{
		HookManager::GetRawFunc<HookFunc::C_HLTVCamera_SetCameraAngle>();
	}
	catch (bad_pointer)
	{
		PluginWarning("Required function C_HLTVCamera::SetCameraAngle for module %s not available!\n", GetModuleName());
		ready = false;
	}

	try
	{
		HookManager::GetRawFunc<HookFunc::C_HLTVCamera_SetMode>();
	}
	catch (bad_pointer)
	{
		PluginWarning("Required function C_HLTVCamera::SetMode for module %s not available!\n", GetModuleName());
		ready = false;
	}

	try
	{
		Interfaces::GetHLTVCamera();
	}
	catch (bad_pointer)
	{
		PluginWarning("Module %s requires C_HLTVCamera, which cannot be verified at this time!\n", GetModuleName());
	}

	return ready;
}

void CameraSmooths::LevelInit()
{
	m_LastSmoothEnd = -FLT_MAX;
}

void CameraSmooths::SetupCameraSmooth(const CameraPtr& currentCamera, CameraPtr& targetCamera)
{
	if (!ce_smoothing_enabled.GetBool())
		return;

	auto currentSmooth = dynamic_cast<ICameraSmooth*>(currentCamera.get());

	const auto clientTime = Interfaces::GetEngineTool()->ClientTime();
	if (m_LastSmoothEnd > clientTime && !currentSmooth)
	{
		if (ce_smoothing_debug.GetBool())
			ConColorMsg(DBGMSG_COLOR, "[%s] Skipping smooth, %s still has %1.2f seconds left\n",
				GetModuleName(), ce_smoothing_cooldown.GetName(), m_LastSmoothEnd - clientTime);

		return;
	}

	Vector currentForward;
	AngleVectors(currentCamera->GetAngles(), &currentForward);

	const Vector deltaPos = targetCamera->GetOrigin() - currentCamera->GetOrigin();
	const float angle = Rad2Deg(std::acosf(currentForward.Dot(deltaPos) / (currentForward.Length() + deltaPos.Length())));
	const float distance = deltaPos.Length();

	if (distance < 0.1f)
	{
		if (ce_smoothing_debug.GetBool())
			ConColorMsg(DBGMSG_COLOR, "[%s] Skipping smooth, start and end points are %1.2f units apart\n", GetModuleName(), distance);

		return;
	}

	const bool forceSmooth = distance <= ce_smoothing_force_distance.GetFloat();
	if (!forceSmooth)
	{
		if (angle > ce_smoothing_fov.GetFloat())
		{
			if (ce_smoothing_debug.GetBool())
				ConColorMsg(DBGMSG_COLOR, "[%s] Skipping smooth, angle difference was %1.0f degrees.\n\n", GetModuleName(), angle);

			return;
		}

		if (ce_smoothing_debug.GetBool())
			ConColorMsg(DBGMSG_COLOR, "[%s] Smooth passed angle test with difference of %1.0f degrees.\n", GetModuleName(), angle);

		const float visibility = TestVisibility(currentCamera->GetOrigin(), targetCamera->GetOrigin());
		if (visibility < ce_smoothing_los_min.GetFloat())
		{
			if (ce_smoothing_debug.GetBool())
				ConColorMsg(DBGMSG_COLOR, "[%s] Skipping smooth, no visibility to new target\n\n", GetModuleName());

			return;
		}

		if (ce_smoothing_debug.GetBool())
			ConColorMsg(DBGMSG_COLOR, "[%s] Smooth passed LOS test (%1.0f%% visible)\n", GetModuleName(), visibility * 100);

		if (ce_smoothing_max_distance.GetFloat() > 0 && distance > ce_smoothing_max_distance.GetFloat())
		{
			if (ce_smoothing_debug.GetBool())
				ConColorMsg(DBGMSG_COLOR, "[%s] Skipping smooth, distance of %1.0f units > %s (%1.0f units)\n\n", GetModuleName(), distance, ce_smoothing_max_distance.GetName(), ce_smoothing_max_distance.GetFloat());

			return;
		}

		if (ce_smoothing_debug.GetBool())
			ConColorMsg(DBGMSG_COLOR, "[%s] Smooth passed distance test, %1.0f units < %s (%1.0f units)\n", GetModuleName(), distance, ce_smoothing_max_distance.GetName(), ce_smoothing_max_distance.GetFloat());
	}
	else
	{
		if (ce_smoothing_debug.GetBool())
			ConColorMsg(DBGMSG_COLOR, "[%s] Forcing smooth, distance of %1.2f units < %s (%1.0f units)\n", GetModuleName(), distance, ce_smoothing_force_distance.GetName(), ce_smoothing_force_distance.GetFloat());
	}

	auto distexp = std::powf(distance, ce_smoothing_rate_dist_exp.GetFloat());
	auto time = distexp / ce_smoothing_rate.GetFloat();

	if (ce_smoothing_debug.GetBool())
		ConColorMsg(DBGMSG_COLOR, "[%s] Launching smooth, time %1.2f, dist %1.2f, dist^exp %1.2f\n", GetModuleName(), time, distance, distexp);

	m_LastSmoothEnd = clientTime + time + ce_smoothing_cooldown.GetFloat();

	auto newSimpleSmooth = std::make_shared<SimpleCameraSmooth>(copy(currentCamera), copy(targetCamera), time);
	newSimpleSmooth->m_Interpolator = Interpolators::Smoothstep;

	// If we're coming from an entity-attached camera and heading for a camera
	// that isn't attached to the same entity (or is attached to nothing)
	if (!currentSmooth &&
		currentCamera->GetAttachedEntity() &&
		currentCamera->GetAttachedEntity() != targetCamera->GetAttachedEntity())
	{
		// If target camera is attached to something
		if (targetCamera->GetAttachedEntity())
			newSimpleSmooth->m_UpdateStartOrigin = false;

		newSimpleSmooth->m_UpdateStartAngles = false;
		newSimpleSmooth->m_UpdateStartFOV = false;
	}

	targetCamera = newSimpleSmooth;
}

float CameraSmooths::TestVisibility(const Vector& eyePos, const Vector& targetPos) const
{
	float visibility = CameraTools::CollisionTest3D(eyePos, targetPos, ce_smoothing_los_buffer.GetFloat(), nullptr);

	if (ce_smoothing_debug_los.GetBool())
	{
		constexpr auto DISPLAY_TIME = 15;

		const Vector buffer(ce_smoothing_los_buffer.GetFloat());
		const auto mins = eyePos - buffer;
		const auto maxs = eyePos + buffer;

		NDebugOverlay::Box(Vector(0, 0, 0), mins, maxs,
			Lerp(visibility, 255, 0),
			Lerp(visibility, 0, 255),
			0,
			64, DISPLAY_TIME);

		char buf[64];
		sprintf_s(buf, "Success rate: %1.1f", visibility * 100);
		NDebugOverlay::Text(targetPos, buf, false, DISPLAY_TIME);
	}

	return visibility;
}
