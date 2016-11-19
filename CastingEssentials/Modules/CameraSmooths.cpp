#include "CameraSmooths.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"
#include "Misc/DebugOverlay.h"
#include "Misc/HLTVCameraHack.h"

#include <convar.h>
#include <client/hltvcamera.h>
#include <shareddefs.h>
#include <toolframework/ienginetool.h>
#include <cdll_int.h>
#include <view_shared.h>
#include <icliententity.h>
#include <engine/IEngineTrace.h>
#include <util_shared.h>

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

CameraSmooths::CameraSmooths()
{
	inToolModeHook = 0;
	isThirdPersonCameraHook = 0;
	setupEngineViewHook = 0;
	smoothEnding = false;
	smoothEndMode = OBS_MODE_NONE;
	smoothEndTarget = 0;
	smoothInProgress = false;
	smoothLastTime = 0;

	enabled = new ConVar("ce_smoothing_enabled", "0", FCVAR_NONE, "smooth transition between camera positions", [](IConVar *var, const char *pOldValue, float flOldValue) { GetModule()->ToggleEnabled(var, pOldValue, flOldValue); });
	max_angle = new ConVar("ce_smoothing_max_angle", "45", FCVAR_NONE, "max angle difference at which smoothing will be performed", true, 0, true, 180);
	max_distance = new ConVar("ce_smoothing_max_distance", "2250", FCVAR_NONE, "max distance at which smoothing will be performed");
	ce_camerasmooths_min_distance = new ConVar("ce_smoothing_min_distance", "128", FCVAR_NONE, "Always smooth if we're closer than this distance.");
	max_speed = new ConVar("ce_smoothing_max_speed", "2000", FCVAR_NONE, "max units per second to move view");
	ce_camerasmooths_duration = new ConVar("ce_smoothing_duration", "0.5", FCVAR_NONE, "Duration over which to smooth the camera.");

	ce_camerasmooths_ang_bias = new ConVar("ce_smoothing_ang_bias", "0.65", FCVAR_NONE, "biasAmt for angle smoothing. 1 = linear, 0 = sharp snap at halfway", true, 0, true, 1);
	ce_camerasmooths_pos_bias = new ConVar("ce_smoothing_pos_bias", "0.75", FCVAR_NONE, "biasAmt for position smoothing. 1 = linear, 0 = sharp snap at halfway", true, 0, true, 1);

	ce_camerasmooths_check_los = new ConVar("ce_smoothing_check_los", "1", FCVAR_NONE, "Make sure we have LOS to the player we're smoothing to.");

	ce_camerasmooths_los_buffer = new ConVar("ce_smoothing_los_buffer", "32", FCVAR_NONE, "Additional space to give ourselves so we can sorta see around corners.");

	ce_camerasmooths_debug_los = new ConVar("ce_smoothing_debug_los", "0", FCVAR_NONE, "Draw points associated with LOS checking for camera smooths.");
	ce_camerasmooths_debug = new ConVar("ce_smoothing_debug", "0", FCVAR_NONE, "Prints debugging info about camera smooths to the console.");

	ce_camerasmooths_avoid_scoped_snipers = new ConVar("ce_smoothing_avoid_scoped_snipers", "0", FCVAR_NONE, "DHW HACK: Don't smooth to scoped snipers.");
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

		const Vector viewPos = m_LastFramePos;

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
			const Vector eyePos = entity->GetAbsOrigin() + VIEW_OFFSETS[(int)playerClass];
			const Vector buffer(ce_camerasmooths_los_buffer->GetFloat());
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

bool CameraSmooths::InToolModeOverride()
{
	if (!Interfaces::GetEngineClient()->IsHLTV())
		return false;

	if (smoothInProgress)
	{
		GetHooks()->SetState<IClientEngineTools_InToolMode>(Hooking::HookAction::SUPERCEDE);
		return true;
	}

	return false;
}

bool CameraSmooths::IsThirdPersonCameraOverride()
{
	if (!Interfaces::GetEngineClient()->IsHLTV())
		return false;

	if (smoothInProgress)
	{
		GetHooks()->SetState<IClientEngineTools_IsThirdPersonCamera>(Hooking::HookAction::SUPERCEDE);
		return true;
	}

	return false;
}

bool CameraSmooths::SetupEngineViewOverride(Vector &origin, QAngle &angles, float &fov)
{
	if (!Interfaces::GetEngineClient()->IsHLTV())
		return false;

	HLTVCameraOverride* const hltvcamera = Interfaces::GetHLTVCamera();

	if (hltvcamera->m_nCameraMode == OBS_MODE_IN_EYE || hltvcamera->m_nCameraMode == OBS_MODE_CHASE)
	{
		if (hltvcamera->m_iTraget1 != smoothEndTarget || (hltvcamera->m_nCameraMode != smoothEndMode && !smoothInProgress))
		{
			smoothEndMode = hltvcamera->m_nCameraMode;
			smoothEndTarget = hltvcamera->m_iTraget1;

			Vector currentForward;
			AngleVectors(m_LastFrameAng, &currentForward);

			const Vector deltaPos = origin - m_LastFramePos;
			const float angle = Rad2Deg(std::acosf(currentForward.Dot(deltaPos) / (currentForward.Length() + deltaPos.Length())));
			const float distance = deltaPos.Length();

			const bool forceSmooth = distance <= ce_camerasmooths_min_distance->GetFloat();
			if (!forceSmooth)
			{
				if (angle > max_angle->GetFloat())
				{
					if (ce_camerasmooths_debug->GetBool())
						ConColorMsg(DBGMSG_COLOR, "[%s] Skipping smooth, angle difference was %1.0f degrees.\n\n", GetModuleName(), angle);
					smoothInProgress = false;
					return false;
				}

				if (ce_camerasmooths_debug->GetBool())
					ConColorMsg(DBGMSG_COLOR, "[%s] Smooth passed angle test with difference of %1.0f degrees.\n", GetModuleName(), angle);

				const float visibility = GetVisibility(smoothEndTarget);
				if (visibility <= 0)
				{
					if (ce_camerasmooths_debug->GetBool())
						ConColorMsg(DBGMSG_COLOR, "[%s] Skipping smooth, no visibility to new target\n\n", GetModuleName());
					smoothInProgress = false;
					return false;
				}

				if (ce_camerasmooths_debug->GetBool())
					ConColorMsg(DBGMSG_COLOR, "[%s] Smooth passed LOS test (%1.0f%% visible)\n", GetModuleName(), visibility * 100);

				if (max_distance->GetFloat() > 0 && distance > max_distance->GetFloat())
				{
					if (ce_camerasmooths_debug->GetBool())
						ConColorMsg(DBGMSG_COLOR, "[%s] Skipping smooth, distance of %1.0f units > %s (%1.0f units)\n\n", GetModuleName(), distance, max_distance->GetName(), max_distance->GetFloat());
					smoothInProgress = false;
					return false;
				}

				if (ce_camerasmooths_debug->GetBool())
					ConColorMsg(DBGMSG_COLOR, "[%s] Smooth passed distance test, %1.0f units < %s (%1.0f units)\n", GetModuleName(), distance, max_distance->GetName(), max_distance->GetFloat());
			}
			else
			{
				if (ce_camerasmooths_debug->GetBool())
					ConColorMsg(DBGMSG_COLOR, "[%s] Forcing smooth, distance of %1.0f units < %s (%1.0f units)\n", GetModuleName(), distance, ce_camerasmooths_min_distance->GetName(), ce_camerasmooths_min_distance->GetFloat());
			}

			if (ce_camerasmooths_debug->GetBool())
				ConColorMsg(DBGMSG_COLOR, "[%s] Launching smooth!\n\n", GetModuleName());

			m_SmoothStartAng = m_LastFrameAng;
			m_SmoothBeginPos = m_SmoothStartPos = m_LastFramePos;
			m_SmoothStartTime = Interfaces::GetEngineTool()->HostTime();
			m_LastOverallProgress = m_LastAngPercentage = 0;
			smoothInProgress = true; // moveVector.Length() < max_distance->GetFloat() &&
										//(max_angle_difference->GetFloat() < 0 || angle < max_angle_difference->GetFloat());
		}
	}
	else
		smoothInProgress = false;

	if (smoothInProgress)
	{
		GetHooks()->SetState<IClientEngineTools_SetupEngineView>(Hooking::HookAction::SUPERCEDE);

		const float percentage = (Interfaces::GetEngineTool()->HostTime() - m_SmoothStartTime) / ce_camerasmooths_duration->GetFloat();
		const float posPercentage = EaseOut(percentage, ce_camerasmooths_pos_bias->GetFloat());

		if (percentage < 1)
		{
			const Vector targetPos = origin;

			// If we had uncapped speed, we'd be here.
			const Vector idealPos = VectorLerp(m_SmoothStartPos, targetPos, posPercentage);

			// How far would we have to travel this frame to get to our ideal position?
			const float posDifference = m_LastFramePos.DistTo(idealPos);

			// What's the furthest we're allowed to travel this frame?
			const float maxDistThisFrame = max_speed->GetFloat() > 0 ?
				max_speed->GetFloat() * Interfaces::GetEngineTool()->HostFrameTime() :
				std::numeric_limits<float>::infinity();

			// Clamp camera translation to max speed
			if (posDifference > maxDistThisFrame)
			{
				m_SmoothStartPos = origin = VectorLerp(m_SmoothStartPos, idealPos, maxDistThisFrame / posDifference);
				m_SmoothStartTime = Interfaces::GetEngineTool()->HostTime();
			}
			else
				origin = idealPos;

			const float overallPercentage = std::max(1 - (origin.DistTo(targetPos) / m_SmoothBeginPos.DistTo(targetPos)), m_LastOverallProgress);
			{
				// Percentage this frame
				const float percentThisFrame = overallPercentage - m_LastOverallProgress;

				const float adjustedPercentage = percentThisFrame / (1 - overallPercentage);

				// Angle percentage is determined by overall progress towards our goal position
				const float angPercentage = EaseIn(overallPercentage, ce_camerasmooths_ang_bias->GetFloat());

				const float angPercentThisFrame = angPercentage - m_LastAngPercentage;
				const float adjustedAngPercentage = angPercentThisFrame / (1 - angPercentage);
				
#if 0
				Quaternion currentForward, endForward;
				AngleQuaternion(percentage == 0 ? m_SmoothStartAng : m_LastFrameAng, currentForward);
				AngleQuaternion(angles, endForward);

				Quaternion lerped;
				QuaternionSlerp(currentForward, endForward, angPercentage, lerped);

				QuaternionAngles(lerped, angles);
#else

				const float distx = AngleDistance(angles.x, m_LastFrameAng.x);
				const float disty = AngleDistance(angles.y, m_LastFrameAng.y);
				const float distz = AngleDistance(angles.z, m_LastFrameAng.z);

				angles.x = ApproachAngle(angles.x, m_LastFrameAng.x, distx * adjustedAngPercentage);
				angles.y = ApproachAngle(angles.y, m_LastFrameAng.y, disty * adjustedAngPercentage);
				angles.z = ApproachAngle(angles.z, m_LastFrameAng.z, distz * adjustedAngPercentage);
#endif
				m_LastAngPercentage = angPercentage;
			}

			m_LastFramePos = origin;
			m_LastFrameAng = angles;
			m_LastOverallProgress = overallPercentage;
			return true;
		}
		else
		{
			smoothInProgress = false;
			smoothEnding = true;
			return true;
		}
	}
	else if (smoothEnding)
	{
		if (hltvcamera)
			GetHooks()->GetFunc<C_HLTVCamera_SetMode>()(smoothEndMode);
	}

	smoothEnding = false;
	smoothEndMode = hltvcamera->m_nCameraMode;
	smoothEndTarget = hltvcamera->m_iTraget1;
	smoothInProgress = false;
	m_LastFrameAng = smoothLastAngles = angles;
	m_LastFramePos = smoothLastOrigin = origin;
	smoothLastTime = Interfaces::GetEngineTool()->HostTime();

	return false;
}

void CameraSmooths::ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue)
{
	if (enabled->GetBool())
	{
		if (!inToolModeHook)
			inToolModeHook = GetHooks()->AddHook<IClientEngineTools_InToolMode>(std::bind(&CameraSmooths::InToolModeOverride, this));

		if (!isThirdPersonCameraHook)
			isThirdPersonCameraHook = GetHooks()->AddHook<IClientEngineTools_IsThirdPersonCamera>(std::bind(&CameraSmooths::IsThirdPersonCameraOverride, this));

		if (!setupEngineViewHook)
			setupEngineViewHook = GetHooks()->AddHook<IClientEngineTools_SetupEngineView>(std::bind(&CameraSmooths::SetupEngineViewOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	}
	else
	{
		if (inToolModeHook && GetHooks()->RemoveHook<IClientEngineTools_InToolMode>(inToolModeHook, __FUNCSIG__))
			inToolModeHook = 0;

		Assert(!inToolModeHook);

		if (isThirdPersonCameraHook && GetHooks()->RemoveHook<IClientEngineTools_IsThirdPersonCamera>(isThirdPersonCameraHook, __FUNCSIG__))
			isThirdPersonCameraHook = 0;

		Assert(!isThirdPersonCameraHook);

		if (setupEngineViewHook && GetHooks()->RemoveHook<IClientEngineTools_SetupEngineView>(setupEngineViewHook, __FUNCSIG__))
			setupEngineViewHook = 0;

		Assert(!setupEngineViewHook);
	}
}

void CameraSmooths::OnTick(bool inGame)
{
	if (inGame)
	{
		{
			CViewSetup view;
			if (Interfaces::GetClientDLL()->GetPlayerView(view))
			{
				m_LastFramePos = view.origin;
				m_LastFrameAng = view.angles;
			}
		}

		if (ce_camerasmooths_debug_los->GetBool())
			DrawCollisionTests();
	}
}