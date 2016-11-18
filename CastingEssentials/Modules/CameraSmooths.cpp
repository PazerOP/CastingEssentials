#include "CameraSmooths.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"

#include <convar.h>
#include <client/hltvcamera.h>
#include <shareddefs.h>
#include <toolframework/ienginetool.h>
#include <cdll_int.h>
#include <view_shared.h>

#undef max
#include <algorithm>

class CameraSmooths::HLTVCameraOverride : public C_HLTVCamera
{
public:
	using C_HLTVCamera::m_nCameraMode;
	using C_HLTVCamera::m_iCameraMan;
	using C_HLTVCamera::m_vCamOrigin;
	using C_HLTVCamera::m_aCamAngle;
	using C_HLTVCamera::m_iTraget1;
	using C_HLTVCamera::m_iTraget2;
	using C_HLTVCamera::m_flFOV;
	using C_HLTVCamera::m_flOffset;
	using C_HLTVCamera::m_flDistance;
	using C_HLTVCamera::m_flLastDistance;
	using C_HLTVCamera::m_flTheta;
	using C_HLTVCamera::m_flPhi;
	using C_HLTVCamera::m_flInertia;
	using C_HLTVCamera::m_flLastAngleUpdateTime;
	using C_HLTVCamera::m_bEntityPacketReceived;
	using C_HLTVCamera::m_nNumSpectators;
	using C_HLTVCamera::m_szTitleText;
	using C_HLTVCamera::m_LastCmd;
	using C_HLTVCamera::m_vecVelocity;
};

static const std::vector<std::shared_ptr<const Vector>>& GetTestPoints()
{
	constexpr int resolution = 6;
	constexpr float mins = -1;
	constexpr float maxs = 1;

	static thread_local std::vector<std::shared_ptr<const Vector>> s_Vectors;
	static thread_local bool s_VectorInitialized = false;

	if (!s_VectorInitialized)
	{
		for (int x = 0; x < resolution; x++)
		{
			const float xpos = RemapVal(x, 0, resolution - 1, mins, maxs);
			for (int y = 0; y < resolution; y++)
			{
				const float ypos = RemapVal(y, 0, resolution - 1, mins, maxs);
				for (int z = 0; z < resolution; z++)
				{
					const float zpos = RemapVal(y, 0, resolution - 1, mins, maxs);

					s_Vectors.push_back(std::make_shared<Vector>(xpos, ypos, zpos));
				}
			}
		}
	}

	return s_Vectors;
}

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

	enabled = new ConVar("ce_camerasmooths_enabled", "0", FCVAR_NONE, "smooth transition between camera positions", [](IConVar *var, const char *pOldValue, float flOldValue) { GetModule()->ToggleEnabled(var, pOldValue, flOldValue); });
	max_angle_difference = new ConVar("ce_camerasmooths_max_angle_difference", "90", FCVAR_NONE, "max angle difference at which smoothing will be performed", true, 0, true, 180);
	max_distance = new ConVar("ce_camerasmooths_max_distance", "-1", FCVAR_NONE, "max distance at which smoothing will be performed");
	max_speed = new ConVar("ce_camerasmooths_max_speed", "2000", FCVAR_NONE, "max units per second to move view");
	ce_camerasmooths_duration = new ConVar("ce_camerasmooths_duration", "0.5", FCVAR_NONE, "Duration over which to smooth the camera.");

	ce_camerasmooths_ang_bias = new ConVar("ce_camerasmooths_ang_bias", "0.65", FCVAR_NONE, "biasAmt for angle smoothing. 1 = linear, 0 = sharp snap at halfway", true, 0, true, 1);
	ce_camerasmooths_pos_bias = new ConVar("ce_camerasmooths_pos_bias", "0.75", FCVAR_NONE, "biasAmt for position smoothing. 1 = linear, 0 = sharp snap at halfway", true, 0, true, 1);
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

bool CameraSmooths::TestCollision(const Vector& currentPos, const Vector& targetPos)
{
	return false;
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

	HLTVCameraOverride* const hltvcamera = (HLTVCameraOverride *)Interfaces::GetHLTVCamera();

	if (hltvcamera->m_nCameraMode == OBS_MODE_IN_EYE || hltvcamera->m_nCameraMode == OBS_MODE_CHASE)
	{
		if (hltvcamera->m_iTraget1 != smoothEndTarget || (hltvcamera->m_nCameraMode != smoothEndMode && !smoothInProgress))
		{
			smoothEndMode = hltvcamera->m_nCameraMode;
			smoothEndTarget = hltvcamera->m_iTraget1;

			Vector targetForward, currentForward;
			AngleVectors(angles, &targetForward);
			AngleVectors(smoothLastAngles, &currentForward);

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
		
#if 0
		if (moveDistance < moveVector.Length() && moveVector.Length() < max_distance->GetFloat() && angle < max_angle_difference->GetFloat())
		{
			float movePercentage = moveDistance / moveVector.Length();

			moveVector *= movePercentage;

			origin = smoothLastOrigin + moveVector;

			angles.x = smoothLastAngles.x + ((angles.x - smoothLastAngles.x) * movePercentage);
			angles.y = smoothLastAngles.y + ((angles.y - smoothLastAngles.y) * movePercentage);
			angles.z = smoothLastAngles.z + ((angles.z - smoothLastAngles.z) * movePercentage);

			GetHooks()->GetFunc<C_HLTVCamera_SetCameraAngle>()(angles);
			hltvcamera->m_vCamOrigin = origin;

			smoothEnding = false;
			smoothInProgress = true;
		}
		else
		{
			if (hltvcamera)
				GetHooks()->GetFunc<C_HLTVCamera_SetMode>()(OBS_MODE_ROAMING);

			smoothEnding = true;
			smoothInProgress = false;
		}

		smoothLastAngles = angles;
		smoothLastOrigin = origin;
		smoothLastTime = Interfaces::GetEngineTool()->HostTime();

		GetHooks()->SetState<IClientEngineTools_SetupEngineView>(Hooking::HookAction::SUPERCEDE);
		return true;
#endif
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
	smoothLastAngles = angles;
	smoothLastOrigin = origin;
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
	}
}