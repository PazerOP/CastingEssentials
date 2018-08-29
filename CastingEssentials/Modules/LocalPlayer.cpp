#include "LocalPlayer.h"
#include "Modules/CameraState.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "Controls/StubPanel.h"

#include <client/c_baseentity.h>
#include <client/c_baseplayer.h>
#include <client/c_baseanimating.h>
#include <cdll_int.h>
#include <debugoverlay_shared.h>
#include <vprof.h>
#include <toolframework/ienginetool.h>
#include <model_types.h>

#include <functional>

MODULE_REGISTER(LocalPlayer);

LocalPlayer::LocalPlayer() :
	ce_localplayer_enabled("ce_localplayer_enabled", "0", FCVAR_NONE, "enable local player override",
		[](IConVar *var, const char*, float) { GetModule()->ToggleEnabled(static_cast<ConVar*>(var)); }),
	ce_localplayer_player("ce_localplayer_player", "0", FCVAR_NONE, "player index to set as the local player"),
	ce_localplayer_set_current_target("ce_localplayer_set_current_target", []() { GetModule()->SetToCurrentTarget(); },
		"set the local player to the current spectator target"),
	ce_localplayer_track_spec_target("ce_localplayer_track_spec_target", "0", FCVAR_NONE, "have the local player value track the spectator target"),

	m_GetLocalPlayerIndexHook(std::bind(&LocalPlayer::GetLocalPlayerIndexOverride, this))
{
}

bool LocalPlayer::CheckDependencies()
{
	bool ready = true;

	if (!Interfaces::GetClientDLL())
	{
		PluginWarning("Required interface IBaseClientDLL for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Interfaces::GetEngineClient())
	{
		PluginWarning("Required interface IVEngineClient for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Interfaces::GetHLTVCamera())
	{
		PluginWarning("Required interface C_HLTVCamera for module %s not available!\n", GetModuleName());
		ready = false;
	}

	try
	{
		HookManager::GetRawFunc<HookFunc::Global_GetLocalPlayerIndex>();
	}
	catch (bad_pointer)
	{
		PluginWarning("Required function GetLocalPlayerIndex for module %s not available!\n", GetModuleName());
		ready = false;
	}

	return ready;
}

int LocalPlayer::GetLocalPlayerIndexOverride()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (ce_localplayer_enabled.GetBool() && Player::IsValidIndex(ce_localplayer_player.GetInt()))
	{
		Player* localPlayer = Player::GetPlayer(ce_localplayer_player.GetInt(), __FUNCSIG__);
		if (localPlayer)
		{
			GetHooks()->GetHook<HookFunc::Global_GetLocalPlayerIndex>()->SetState(Hooking::HookAction::SUPERCEDE);
			return ce_localplayer_player.GetInt();
		}
	}

	return 0;
}

void LocalPlayer::ToggleEnabled(const ConVar *var)
{
	m_GetLocalPlayerIndexHook.SetEnabled(var->GetBool());
}

void LocalPlayer::SetToCurrentTarget()
{
	auto localMode = CameraState::GetModule()->GetLocalObserverMode();

	if (localMode == OBS_MODE_FIXED ||
		localMode == OBS_MODE_IN_EYE ||
		localMode == OBS_MODE_CHASE)
	{
		Player* targetPlayer = Player::AsPlayer(CameraState::GetModule()->GetLocalObserverTarget());

		if (targetPlayer)
		{
			ce_localplayer_player.SetValue(targetPlayer->GetEntity()->entindex());
			return;
		}
	}

	ce_localplayer_player.SetValue(Interfaces::GetEngineClient()->GetLocalPlayer());
}

void LocalPlayer::OnTick(bool inGame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);

	if (!inGame)
		return;

	if (ce_localplayer_track_spec_target.GetBool())
		SetToCurrentTarget();
}
