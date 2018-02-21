#include "CameraSmooths.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"
#include "Misc/DebugOverlay.h"
#include "Misc/HLTVCameraHack.h"
#include "Modules/CameraState.h"

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

static const Vector TEST_POINTS[27] =
{
	Vector(0, 0, 0),
	Vector(0, 0, 0.5),
	Vector(0, 0, 1),

	Vector(0, 0.5, 0),
	Vector(0, 0.5, 0.5),
	Vector(0, 0.5, 1),

	Vector(0, 1, 0),
	Vector(0, 1, 0.5),
	Vector(0, 1, 1),

	Vector(0.5, 0, 0),
	Vector(0.5, 0, 0.5),
	Vector(0.5, 0, 1),

	Vector(0.5, 0.5, 0),
	Vector(0.5, 0.5, 0.5),
	Vector(0.5, 0.5, 1),

	Vector(0.5, 1, 0),
	Vector(0.5, 1, 0.5),
	Vector(0.5, 1, 1),

	Vector(1, 0, 0),
	Vector(1, 0, 0.5),
	Vector(1, 0, 1),

	Vector(1, 0.5, 0),
	Vector(1, 0.5, 0.5),
	Vector(1, 0.5, 1),

	Vector(1, 1, 0),
	Vector(1, 1, 0.5),
	Vector(1, 1, 1),
};

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
	m_EndMode = OBS_MODE_NONE;
	m_EndTarget = 0;
	m_InProgress = false;
	m_LastHostTime = 0;
}

bool CameraSmooths::CheckDependencies()
{
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
		GetHooks()->GetFunc<C_HLTVCamera_SetCameraAngle>();
	}
	catch (bad_pointer)
	{
		PluginWarning("Required function C_HLTVCamera::SetCameraAngle for module %s not available!\n", GetModuleName());
		ready = false;
	}

	try
	{
		GetHooks()->GetFunc<C_HLTVCamera_SetMode>();
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

void CameraSmooths::UpdateCollisionTests()
{
	const auto currentFrame = Interfaces::GetEngineTool()->HostFrameCount();
	if (m_CollisionTestFrame != currentFrame)
	{
		m_CollisionTests.clear();

		const Vector& viewPos = CameraState::GetModule()->GetLastFramePluginViewOrigin();

		for (Player* player : Player::Iterable())
		{
			const auto team = player->GetTeam();
			if (team != TFTeam::Red && team != TFTeam::Blue)
				continue;

			if (!player->IsAlive())
				continue;

			IClientEntity* const entity = player->GetEntity();
			if (!entity)
				continue;

			CollisionTest newTest;
			newTest.m_Entity = entity->GetRefEHandle();

			const TFClassType playerClass = player->GetClass();
			const Vector eyePos = player->GetEyePosition();
			const Vector buffer(ce_smoothing_los_buffer.GetFloat());
			newTest.m_Mins = eyePos - buffer;
			newTest.m_Maxs = eyePos + buffer;

			const Vector delta = newTest.m_Maxs - newTest.m_Mins;

			size_t pointsPassed = 0;
			for (const auto& testPoint : TEST_POINTS)
			{
				const Vector worldTestPoint = newTest.m_Mins + delta * testPoint;

				trace_t tr;
				UTIL_TraceLine(viewPos, worldTestPoint, MASK_VISIBLE, entity, COLLISION_GROUP_NONE, &tr);

				if (tr.fraction >= 1)
					pointsPassed++;
			}

			newTest.m_Visibility = (float)pointsPassed / arraysize(TEST_POINTS);

			m_CollisionTests.push_back(newTest);
		}

		m_CollisionTestFrame = currentFrame;
	}
}

void CameraSmooths::DrawCollisionTests()
{
	UpdateCollisionTests();

	for (const CollisionTest& test : m_CollisionTests)
	{
		C_BaseEntity* entity = test.m_Entity.Get();
		if (!entity)
			continue;

		NDebugOverlay::Box(Vector(0, 0, 0), test.m_Mins, test.m_Maxs,
			Lerp(test.m_Visibility, 255, 0),
			Lerp(test.m_Visibility, 0, 255),
			0,
			64, 0);

		const std::string success = strprintf("Success rate: %1.1f", test.m_Visibility * 100);
		NDebugOverlay::Text(entity->GetAbsOrigin(), success.c_str(), false, 0);
	}
}

float CameraSmooths::GetVisibility(int entIndex)
{
	UpdateCollisionTests();
	for (const auto& test : m_CollisionTests)
	{
		if (test.m_Entity.GetEntryIndex() != entIndex)
			continue;

		return test.m_Visibility;
	}

	return 0;
}

// Returns the distance to target. See https://www.desmos.com/calculator/fspe3a4hd6
static float ComputeSmooth(float time, float startTime, float totalDist, float maxSpeed, float bezierDist, float bezierDuration, float& percent)
{
	// x = time in seconds
	// y = distance to target

	const float slope = maxSpeed;

	constexpr float x0 = 0;
	constexpr float y0 = 0;

	const float y2 = totalDist;
	const float x2 = y2 / slope;

	const float x3 = x2 + bezierDuration;
	const float y3 = y2;

	if (totalDist <= bezierDist)
	{
		constexpr float splitPoint = 0;

		constexpr float x1 = 0;
		constexpr float y1 = 0;

		percent = (time - startTime) / x3;

		return totalDist - Bezier(percent, y1, y2, y3);
	}
	else
	{
		const float splitPoint = 1 - (bezierDist / totalDist);

		const float x1 = splitPoint * (y3 / slope);
		const float y1 = splitPoint * totalDist;

		const float bezierBeginX = x1;
		//const float bezierBeginY = y1;

		const float t = time - startTime;
		percent = t / x3;

		if (t < bezierBeginX)
		{
			// Simple linear interp between 0 and y1
			const float lerp = Lerp<float>(t / bezierBeginX, 0, y1);
			return totalDist - lerp;
		}
		else
		{
			const float bezier = Bezier((t - bezierBeginX) / (x3 - bezierBeginX), y1, y2, y3);
			return totalDist - bezier;
		}
	}
}

bool CameraSmooths::InToolModeOverride() const
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (!Interfaces::GetEngineClient()->IsHLTV())
		return false;

	if (m_InProgress)
		return true;

	return false;
}

bool CameraSmooths::IsThirdPersonCameraOverride() const
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (!Interfaces::GetEngineClient()->IsHLTV())
		return false;

	if (m_InProgress)
		return true;

	return false;
}

bool CameraSmooths::SetupEngineViewOverride(Vector &origin, QAngle &angles, float &fov)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (!ce_smoothing_enabled.GetBool())
		return false;

	if (!Interfaces::GetEngineClient()->IsHLTV())
		return false;

	HLTVCameraOverride* const hltvcamera = Interfaces::GetHLTVCamera();

	const Vector& lastFramePos = CameraState::GetModule()->GetLastFramePluginViewOrigin();
	const QAngle& lastFrameAng = CameraState::GetModule()->GetLastFramePluginViewAngles();

	const float frametime = Interfaces::GetEngineTool()->HostFrameTime();
	const float hosttime = Interfaces::GetEngineTool()->HostTime();

	if (hltvcamera->m_nCameraMode == OBS_MODE_IN_EYE || hltvcamera->m_nCameraMode == OBS_MODE_CHASE)
	{
		if (hltvcamera->m_iTraget1 != m_EndTarget || (hltvcamera->m_nCameraMode != m_EndMode && !m_InProgress))
		{
			m_EndMode = hltvcamera->m_nCameraMode;
			m_EndTarget = hltvcamera->m_iTraget1;

			Vector currentForward;
			AngleVectors(lastFrameAng, &currentForward);

			const Vector deltaPos = origin - lastFramePos;
			const float angle = Rad2Deg(std::acosf(currentForward.Dot(deltaPos) / (currentForward.Length() + deltaPos.Length())));
			const float distance = deltaPos.Length();

			const bool forceSmooth = distance <= ce_smoothing_force_distance.GetFloat();
			if (!forceSmooth)
			{
				if (angle > ce_smoothing_fov.GetFloat())
				{
					if (ce_smoothing_debug.GetBool())
						ConColorMsg(DBGMSG_COLOR, "[%s] Skipping smooth, angle difference was %1.0f degrees.\n\n", GetModuleName(), angle);
					m_InProgress = false;
					return false;
				}

				if (ce_smoothing_debug.GetBool())
					ConColorMsg(DBGMSG_COLOR, "[%s] Smooth passed angle test with difference of %1.0f degrees.\n", GetModuleName(), angle);

				const float visibility = GetVisibility(m_EndTarget);
				if (visibility <= ce_smoothing_los_min.GetFloat())
				{
					if (ce_smoothing_debug.GetBool())
						ConColorMsg(DBGMSG_COLOR, "[%s] Skipping smooth, no visibility to new target\n\n", GetModuleName());
					m_InProgress = false;
					return false;
				}

				if (ce_smoothing_debug.GetBool())
					ConColorMsg(DBGMSG_COLOR, "[%s] Smooth passed LOS test (%1.0f%% visible)\n", GetModuleName(), visibility * 100);

				if (ce_smoothing_max_distance.GetFloat() > 0 && distance > ce_smoothing_max_distance.GetFloat())
				{
					if (ce_smoothing_debug.GetBool())
						ConColorMsg(DBGMSG_COLOR, "[%s] Skipping smooth, distance of %1.0f units > %s (%1.0f units)\n\n", GetModuleName(), distance, ce_smoothing_max_distance.GetName(), ce_smoothing_max_distance.GetFloat());
					m_InProgress = false;
					return false;
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

			m_SmoothStartAng = lastFrameAng;
			m_SmoothStartPos = lastFramePos;
			m_StartDist = m_SmoothStartPos.DistTo(origin);
			m_SmoothStartTime = m_LastHostTime;
			m_LastOverallProgress = m_LastAngPercentage = 0;
			m_InProgress = true;
		}
	}
	else
		m_InProgress = false;

	if (m_InProgress)
	{
		const Vector targetPos = origin;
		const float distToTarget = lastFramePos.DistTo(targetPos);

		float percent;

		const float targetDist = ComputeSmooth(
			hosttime, m_SmoothStartTime,
			m_StartDist,
			ce_smoothing_linear_speed.GetFloat(),
			ce_smoothing_bezier_dist.GetFloat(),
			ce_smoothing_bezier_duration.GetFloat(),
			percent);

		if (ce_smoothing_debug.GetBool())
		{
			GetConLine();
			engine->Con_NPrintf(GetConLine(), "%%: %1.1f", percent * 100);
			engine->Con_NPrintf(GetConLine(), "targetDist: %1.0f", targetDist);
		}

		if (percent >= 1)
		{
			m_InProgress = false;

			if (hltvcamera)
				GetHooks()->GetFunc<C_HLTVCamera_SetMode>()(m_EndMode);
		}
		else
		{
			if (ce_smoothing_mode.GetInt() == 0)
				origin = ApproachVector(targetPos, lastFramePos, targetDist);
			else if (ce_smoothing_mode.GetInt() == 1)
				origin = VectorLerp(m_SmoothStartPos, targetPos, RemapVal(targetDist, m_StartDist, 0, 0, 1));

			// Angles
			{
				// Percentage this frame
				const float percentThisFrame = percent - m_LastOverallProgress;

				const float adjustedPercentage = percentThisFrame / (1 - percent);

				// Angle percentage is determined by overall progress towards our goal position
				const float angPercentage = EaseIn(percent, ce_smoothing_ang_bias.GetFloat());

				const float angPercentThisFrame = angPercentage - m_LastAngPercentage;
				const float adjustedAngPercentage = angPercentThisFrame / (1 - angPercentage);

				const float distx = AngleDistance(angles.x, lastFrameAng.x);
				const float disty = AngleDistance(angles.y, lastFrameAng.y);
				const float distz = AngleDistance(angles.z, lastFrameAng.z);

				angles.x = ApproachAngle(angles.x, lastFrameAng.x, distx * adjustedAngPercentage);
				angles.y = ApproachAngle(angles.y, lastFrameAng.y, disty * adjustedAngPercentage);
				angles.z = ApproachAngle(angles.z, lastFrameAng.z, distz * adjustedAngPercentage);

				m_LastAngPercentage = angPercentage;
				m_LastOverallProgress = percent;
			}

			return true;
		}
	}

	m_EndMode = hltvcamera->m_nCameraMode;
	m_EndTarget = hltvcamera->m_iTraget1;
	m_InProgress = false;
	m_LastHostTime = hosttime;

	return false;
}

void CameraSmooths::OnTick(bool inGame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (inGame)
	{
		if (ce_smoothing_debug_los.GetBool())
			DrawCollisionTests();
	}
}