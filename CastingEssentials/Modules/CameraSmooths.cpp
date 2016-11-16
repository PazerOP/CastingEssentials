#include "CameraSmooths.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"

#include <convar.h>
#include <client/hltvcamera.h>
#include <shareddefs.h>
#include <toolframework/ienginetool.h>
#include <cdll_int.h>

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
	max_angle_difference = new ConVar("ce_camerasmooths_max_angle_difference", "90", FCVAR_NONE, "max angle difference at which smoothing will be performed");
	max_distance = new ConVar("ce_camerasmooths_max_distance", "800", FCVAR_NONE, "max distance at which smoothing will be performed");
	move_speed = new ConVar("ce_camerasmooths_move_speed", "800", FCVAR_NONE, "speed to move view per second");
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

			Vector moveVector = origin - smoothLastOrigin;
			Vector currentAngleVector(smoothLastAngles.x, smoothLastAngles.y, smoothLastAngles.z);
			Vector targetAngleVector(angles.x, angles.y, angles.z);

			float angle = acos(currentAngleVector.Dot(targetAngleVector) / (currentAngleVector.Length() * targetAngleVector.Length())) * 180.f / M_PI_F;

			smoothInProgress = moveVector.Length() < max_distance->GetFloat() && angle < max_angle_difference->GetFloat();
		}
	}
	else
		smoothInProgress = false;

	if (smoothInProgress)
	{
		float moveDistance = move_speed->GetFloat() * (Interfaces::GetEngineTool()->HostTime() - smoothLastTime);

		Vector moveVector = origin - smoothLastOrigin;
		Vector currentAngleVector(smoothLastAngles.x, smoothLastAngles.y, smoothLastAngles.z);
		Vector targetAngleVector(angles.x, angles.y, angles.z);

		float angle = acos(currentAngleVector.Dot(targetAngleVector) / (currentAngleVector.Length() * targetAngleVector.Length())) * 180.f / M_PI_F;

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