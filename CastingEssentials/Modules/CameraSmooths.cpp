#include "CameraSmooths.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"
#include "Misc/DebugOverlay.h"
#include "Misc/HLTVCameraHack.h"
#include "Modules/Camera/HybridPlayerCameraSmooth.h"
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

CameraSmooths::CameraSmooths() :
	ce_smoothing_enabled("ce_smoothing_enabled", "0", FCVAR_NONE, "Enables smoothing between spectator targets."),
	ce_smoothing_fov("ce_smoothing_fov", "45", FCVAR_NONE, "Only targets within this FOV will be smoothed to.", true, 0, true, 180),
	ce_smoothing_max_distance("ce_smoothing_max_distance", "2250", FCVAR_NONE, "max distance at which smoothing will be performed"),
	ce_smoothing_force_distance("ce_smoothing_force_distance", "128", FCVAR_NONE, "Always smooth if we're closer than this distance."),

	ce_smoothing_linear_speed("ce_smoothing_linear_speed", "875", FCVAR_NONE, "Speed at which to approach the bezier curve start."),
	ce_smoothing_bezier_dist("ce_smoothing_bezier_dist", "1000", FCVAR_NONE, "Units from target to begin the bezier smooth."),
	ce_smoothing_bezier_duration("ce_smoothing_bezier_duration", "0.65", FCVAR_NONE, "Duration over which to smooth the camera."),

	ce_smoothing_ang_bias("ce_smoothing_ang_bias", "0.85", FCVAR_NONE, "biasAmt for angle smoothing. 1 = linear, 0 = sharp snap at halfway", true, 0, true, 1),

	ce_smoothing_mode("ce_smoothing_mode", "1", FCVAR_NONE,
		"\nDifferent modes for smoothing to targets."
		"\n\t0: Clamp distance to target."
		"\n\t1: Lerp between start position and target.",
		true, 0, true, 1),

	ce_smoothing_debug("ce_smoothing_debug", "0", FCVAR_NONE, "Prints debugging info about camera smooths to the console."),
	ce_smoothing_debug_los("ce_smoothing_debug_los", "0", FCVAR_NONE, "Draw points associated with LOS checking for camera smooths."),

	ce_smoothing_check_los("ce_smoothing_check_los", "1", FCVAR_NONE, "Make sure we have LOS to the player we're smoothing to."),
	ce_smoothing_los_buffer("ce_smoothing_los_buffer", "32", FCVAR_NONE, "Additional space to give ourselves so we can sorta see around corners."),
	ce_smoothing_los_min("ce_smoothing_los_min", "0", FCVAR_NONE, "Minimum percentage of points that must pass the LOS check before we allow ourselves to smooth to a target.", true, 0, true, 1)
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

std::shared_ptr<ICamera> CameraSmooths::CreatePlayerSmooth(const std::shared_ptr<ICamera>& currentCam, const std::shared_ptr<ICamera>& newCam) const
{
	if (!ce_smoothing_enabled.GetBool())
		return newCam;

	if (!Interfaces::GetEngineClient()->IsHLTV())
		return newCam;

	const float frametime = Interfaces::GetEngineTool()->HostFrameTime();
	const float hosttime = Interfaces::GetEngineTool()->HostTime();

	Vector currentForward;
	AngleVectors(currentCam->GetAngles(), &currentForward);

	const Vector deltaPos = newCam->GetOrigin() - currentCam->GetOrigin();
	const float angle = Rad2Deg(std::acosf(currentForward.Dot(deltaPos) / (currentForward.Length() + deltaPos.Length())));
	const float distance = deltaPos.Length();

	const bool forceSmooth = distance <= ce_smoothing_force_distance.GetFloat();
	if (!forceSmooth)
	{
		if (angle > ce_smoothing_fov.GetFloat())
		{
			if (ce_smoothing_debug.GetBool())
				ConColorMsg(DBGMSG_COLOR, "[%s] Skipping smooth, angle difference was %1.0f degrees.\n\n", GetModuleName(), angle);

			return newCam;
		}

		if (ce_smoothing_debug.GetBool())
			ConColorMsg(DBGMSG_COLOR, "[%s] Smooth passed angle test with difference of %1.0f degrees.\n", GetModuleName(), angle);

		const float visibility = TestVisibility(currentCam->GetOrigin(), newCam->GetOrigin());
		if (visibility <= ce_smoothing_los_min.GetFloat())
		{
			if (ce_smoothing_debug.GetBool())
				ConColorMsg(DBGMSG_COLOR, "[%s] Skipping smooth, no visibility to new target\n\n", GetModuleName());

			return newCam;
		}

		if (ce_smoothing_debug.GetBool())
			ConColorMsg(DBGMSG_COLOR, "[%s] Smooth passed LOS test (%1.0f%% visible)\n", GetModuleName(), visibility * 100);

		if (ce_smoothing_max_distance.GetFloat() > 0 && distance > ce_smoothing_max_distance.GetFloat())
		{
			if (ce_smoothing_debug.GetBool())
				ConColorMsg(DBGMSG_COLOR, "[%s] Skipping smooth, distance of %1.0f units > %s (%1.0f units)\n\n", GetModuleName(), distance, ce_smoothing_max_distance.GetName(), ce_smoothing_max_distance.GetFloat());

			return newCam;
		}

		if (ce_smoothing_debug.GetBool())
			ConColorMsg(DBGMSG_COLOR, "[%s] Smooth passed distance test, %1.0f units < %s (%1.0f units)\n", GetModuleName(), distance, ce_smoothing_max_distance.GetName(), ce_smoothing_max_distance.GetFloat());
	}
	else
	{
		if (ce_smoothing_debug.GetBool())
			ConColorMsg(DBGMSG_COLOR, "[%s] Forcing smooth, distance of %1.0f units < %s (%1.0f units)\n", GetModuleName(), distance, ce_smoothing_force_distance.GetName(), ce_smoothing_force_distance.GetFloat());
	}

	if (ce_smoothing_debug.GetBool())
		ConColorMsg(DBGMSG_COLOR, "[%s] Launching smooth!\n\n", GetModuleName());

	auto newSmooth = std::make_shared<HybridPlayerCameraSmooth>(copy(currentCam), copy(newCam));
	newSmooth->m_AngleBias = ce_smoothing_ang_bias.GetFloat();
	newSmooth->m_BezierDist = ce_smoothing_bezier_dist.GetFloat();
	newSmooth->m_BezierDuration = ce_smoothing_bezier_duration.GetFloat();
	newSmooth->m_LinearSpeed = ce_smoothing_linear_speed.GetFloat();
	newSmooth->m_SmoothingMode = (HybridPlayerCameraSmooth::SmoothingMode)ce_smoothing_mode.GetInt();

	return newSmooth;
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