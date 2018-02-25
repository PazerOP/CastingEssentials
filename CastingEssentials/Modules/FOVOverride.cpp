#include "fovoverride.h"

#include <client/c_baseentity.h>
#include <client/c_baseplayer.h>
#include <icliententity.h>
#include <shareddefs.h>
#include <vprof.h>

#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"
#include "Misc/HLTVCameraHack.h"
#include "Modules/CameraState.h"
#include "Modules/CameraTools.h"

FOVOverride::FOVOverride() :
	ce_fovoverride_firstperson("ce_fovoverride_firstperson", "90"),
	ce_fovoverride_thirdperson("ce_fovoverride_thirdperson", "90"),
	ce_fovoverride_roaming("ce_fovoverride_roaming", "90"),
	ce_fovoverride_fixed("ce_fovoverride_fixed", "90"),
	ce_fovoverride_test("ce_fovoverride_test", "90", FCVAR_NONE, "FOV override to apply in all cases. Only enabled if ce_fovoverride_all_enabled is nonzero. Designed to help with the placement of autocameras."),
	ce_fovoverride_test_enabled("ce_fovoverride_test_enabled", "0", FCVAR_NONE, "Enables ce_fovoverride_test.")
{
	m_InToolModeHook = 0;
	m_SetupEngineViewHook = 0;
	m_GetDefaultFOVHook = 0;
}

bool FOVOverride::CheckDependencies()
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

	if (!Player::CheckDependencies())
	{
		PluginWarning("Required player helper class for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Player::IsConditionsRetrievalAvailable())
	{
		PluginWarning("Required player condition retrieval for module %s not available!\n", GetModuleName());
		ready = false;
	}

	return ready;
}

float FOVOverride::GetBaseFOV(ObserverMode mode) const
{
	float fov = 0;
	switch (mode)
	{
		case OBS_MODE_IN_EYE:  fov = ce_fovoverride_firstperson.GetFloat(); break;
		case OBS_MODE_CHASE:   fov = ce_fovoverride_thirdperson.GetFloat(); break;
		case OBS_MODE_FIXED:   fov = ce_fovoverride_fixed.GetFloat(); break;
		case OBS_MODE_ROAMING: fov = ce_fovoverride_roaming.GetFloat(); break;
	}

	if (fov == 0)
	{
		static ConVarRef fov_desired("fov_desired");
		fov = fov_desired.GetFloat();
	}

	return fov;
}

void FOVOverride::OnTick(bool inGame)
{
	if (inGame)
	{
		if (!m_InToolModeHook)
			m_InToolModeHook = GetHooks()->AddHook<IClientEngineTools_InToolMode>(std::bind(&FOVOverride::InToolModeOverride, this));

		if (!m_SetupEngineViewHook)
			m_SetupEngineViewHook = GetHooks()->AddHook<IClientEngineTools_SetupEngineView>(std::bind(&FOVOverride::SetupEngineViewOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	}
	else
	{
		if (m_InToolModeHook && GetHooks()->RemoveHook<IClientEngineTools_InToolMode>(m_InToolModeHook, __FUNCSIG__))
			m_InToolModeHook = 0;

		Assert(!m_InToolModeHook);

		if (m_SetupEngineViewHook && GetHooks()->RemoveHook<IClientEngineTools_SetupEngineView>(m_SetupEngineViewHook, __FUNCSIG__))
			m_SetupEngineViewHook = 0;

		Assert(!m_SetupEngineViewHook);
	}
}

bool FOVOverride::InToolModeOverride()
{
	GetHooks()->SetState<IClientEngineTools_InToolMode>(Hooking::HookAction::SUPERCEDE);
	return true;
}

bool FOVOverride::SetupEngineViewOverride(Vector&, QAngle&, float &fov)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);

	if (ce_fovoverride_test_enabled.GetBool())
	{
		fov = ce_fovoverride_test.GetFloat();
	}
	else
	{
		auto camTools = CameraTools::GetModule();

		switch (CameraState::GetObserverMode())
		{
			case OBS_MODE_IN_EYE:
			{
				auto newFov = ce_fovoverride_firstperson.GetFloat();
				if (newFov == 0)
					return false;

				auto target = Player::GetLocalObserverTarget();
				if (!target || target->CheckCondition(TFCond_Zoomed))
					return false;

				fov = newFov;
				break;
			}
			case OBS_MODE_CHASE:
			{
				auto newFov = ce_fovoverride_thirdperson.GetFloat();
				if (newFov == 0)
					return false;

				fov = newFov;
				break;
			}
			case OBS_MODE_ROAMING:
			{
				if (camTools->GetModeSwitchReason() == ModeSwitchReason::SpecPosition)
					return false;

				auto newFov = ce_fovoverride_roaming.GetFloat();
				if (newFov == 0)
					return false;

				fov = newFov;
				break;
			}

			case OBS_MODE_FIXED:
			{
				if (camTools->GetModeSwitchReason() == ModeSwitchReason::SpecPosition)
					return false;

				auto newFov = ce_fovoverride_fixed.GetFloat();
				if (newFov == 0)
					return false;

				fov = newFov;
				break;
			}

			default:
				return false;
		}
	}

#if 0
	if (Interfaces::GetEngineClient()->IsHLTV())
	{
		const auto target = Interfaces::GetHLTVCamera()->m_iTraget1;
		if (!Player::IsValidIndex(target))
			return false;

		Player* const targetPlayer = Player::GetPlayer(target, __FUNCSIG__);
		if (targetPlayer)
		{
			if (auto basePlayer = targetPlayer->GetBasePlayer())
			{
				//basePlayer->m_iDefaultFOV = std::lroundf(ce_fovoverride_fov.GetFloat());
				engine->Con_NPrintf(0, "GetFOV(): %f", GetHooks()->GetRawFunc_C_BasePlayer_GetFOV()(basePlayer));
				engine->Con_NPrintf(1, "m_iFOV: %i", basePlayer->m_iFOV);
				engine->Con_NPrintf(2, "m_iFOVStart: %i", basePlayer->m_iFOVStart);
				engine->Con_NPrintf(3, "m_flFOVTime: %f", basePlayer->m_flFOVTime);
				engine->Con_NPrintf(4, "m_iDefaultFOV: %i", basePlayer->m_iDefaultFOV);
				engine->Con_NPrintf(5, "hltv fov: %f", Interfaces::GetHLTVCamera()->m_flFOV);
				return false;
			}
		}

		if (Interfaces::GetHLTVCamera()->GetMode() == OBS_MODE_IN_EYE && targetPlayer && targetPlayer->CheckCondition(TFCond_Zoomed))
			return false;
	}
	else
	{
		Player* const localPlayer = Player::GetPlayer(Interfaces::GetEngineClient()->GetLocalPlayer(), __FUNCSIG__);
		if (localPlayer)
		{
			if (localPlayer->CheckCondition(TFCond_Zoomed))
				return false;
			else if (localPlayer->GetObserverMode() == OBS_MODE_IN_EYE)
			{
				Player* targetPlayer = Player::AsPlayer(localPlayer->GetObserverTarget());

				if (targetPlayer && targetPlayer->CheckCondition(TFCond_Zoomed))
					return false;
			}
		}
	}
#endif

	//fov = ce_fovoverride_fov.GetFloat();
	GetHooks()->SetState<IClientEngineTools_SetupEngineView>(Hooking::HookAction::SUPERCEDE);
	return true;
}