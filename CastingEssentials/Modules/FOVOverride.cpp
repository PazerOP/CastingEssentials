#include "fovoverride.h"

#include <client/c_baseentity.h>
#include <client/c_baseplayer.h>
#include <convar.h>
#include <icliententity.h>
#include <shareddefs.h>

#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"

FOVOverride::FOVOverride()
{
	inToolModeHook = 0;
	setupEngineViewHook = 0;

	enabled = new ConVar("ce_fovoverride_enabled", "0", FCVAR_NONE, "enable FOV override", [](IConVar *var, const char *pOldValue, float flOldValue) { GetModule()->ToggleEnabled(var, pOldValue, flOldValue); });
	m_FOV = new ConVar("ce_fovoverride_fov", "90", FCVAR_NONE, "the FOV value used");
	zoomed = new ConVar("ce_fovoverride_zoomed", "0", FCVAR_NONE, "enable FOV override even when sniper rifle is zoomed");
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

bool FOVOverride::InToolModeOverride()
{
	GetHooks()->SetState<IClientEngineTools_InToolMode>(Hooking::HookAction::SUPERCEDE);
	return true;
}

bool FOVOverride::SetupEngineViewOverride(Vector &origin, QAngle &angles, float &fov)
{
	if (!Interfaces::GetEngineClient()->IsInGame())
	{
		GetHooks()->SetState<IClientEngineTools_SetupEngineView>(Hooking::HookAction::IGNORE);
		return false;
	}

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

	fov = m_FOV->GetFloat();
	GetHooks()->SetState<IClientEngineTools_SetupEngineView>(Hooking::HookAction::SUPERCEDE);
	return true;
}

void FOVOverride::ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue)
{
	if (enabled->GetBool())
	{
		if (!inToolModeHook)
			inToolModeHook = GetHooks()->AddHook<IClientEngineTools_InToolMode>(std::bind(&FOVOverride::InToolModeOverride, this));

		if (!setupEngineViewHook)
			setupEngineViewHook = GetHooks()->AddHook<IClientEngineTools_SetupEngineView>(std::bind(&FOVOverride::SetupEngineViewOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	}
	else
	{
		if (inToolModeHook && GetHooks()->RemoveHook<IClientEngineTools_InToolMode>(inToolModeHook, __FUNCSIG__))
			inToolModeHook = 0;

		Assert(!inToolModeHook);

		if (setupEngineViewHook && GetHooks()->RemoveHook<IClientEngineTools_SetupEngineView>(setupEngineViewHook, __FUNCSIG__))
			setupEngineViewHook = 0;

		Assert(!setupEngineViewHook);
	}
}