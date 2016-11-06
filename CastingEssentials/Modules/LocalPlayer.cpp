#include "LocalPlayer.h"
#include "PluginBase/Funcs.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "Controls/StubPanel.h"

#include <client/c_baseentity.h>
#include <cdll_int.h>
#include <convar.h>

#include <functional>

class LocalPlayer::TickPanel final : public virtual vgui::StubPanel
{
public:
	TickPanel(std::function<void()> setFunction)
	{
		m_SetToCurrentTarget = setFunction;
	}

	void OnTick() override;
private:
	std::function<void()> m_SetToCurrentTarget;
};

LocalPlayer::LocalPlayer()
{
	m_GetLocalPlayerIndexHookID = 0;

	enabled = new ConVar("ce_localplayer_enabled", "0", FCVAR_NONE, "enable local player override", [](IConVar *var, const char *pOldValue, float flOldValue) { GetModule()->ToggleEnabled(var, pOldValue, flOldValue); });
	player = new ConVar("ce_localplayer_player", "0", FCVAR_NONE, "player index to set as the local player");
	set_current_target = new ConCommand("ce_localplayer_set_current_target", []() { GetModule()->SetToCurrentTarget(); }, "set the local player to the current spectator target", FCVAR_NONE);
	track_spec_target = new ConVar("ce_localplayer_track_spec_target", "0", FCVAR_NONE, "have the local player value track the spectator target", [](IConVar *var, const char *pOldValue, float flOldValue) { GetModule()->ToggleTrackSpecTarget(var, pOldValue, flOldValue); });
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

	if (!Player::IsConditionsRetrievalAvailable())
	{
		PluginWarning("Required player condition retrieval for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Interfaces::GetHLTVCamera())
	{
		PluginWarning("Required interface C_HLTVCamera for module %s not available!\n", GetModuleName());
		ready = false;
	}

	try
	{
		Funcs::GetFunc_Global_GetLocalPlayerIndex();
	}
	catch (bad_pointer)
	{
		PluginWarning("Required function GetLocalPlayerIndex for module %s not available!\n", GetModuleName());
		ready = false;
	}

	return ready;
}

void LocalPlayer::ToggleTrackSpecTarget(IConVar *var, const char *pOldValue, float flOldValue)
{
	if (track_spec_target->GetBool())
	{
		if (!m_Panel)
			m_Panel.reset(new TickPanel(std::bind(&LocalPlayer::SetToCurrentTarget, this)));
	}
	else
	{
		if (m_Panel)
			m_Panel.reset();
	}
}

int LocalPlayer::GetLocalPlayerIndexOverride()
{
	if (enabled->GetBool() && Player::IsValidIndex(player->GetInt()))
	{
		Player* localPlayer = Player::GetPlayer(player->GetInt(), __FUNCSIG__);
		if (localPlayer)
		{
			Funcs::GetHook_Global_GetLocalPlayerIndex()->SetState(Hooking::HookAction::SUPERCEDE);
			return player->GetInt();
		}
	}

	Funcs::GetHook_Global_GetLocalPlayerIndex()->SetState(Hooking::HookAction::IGNORE);
	return 0;
}

void LocalPlayer::ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue)
{
	if (enabled->GetBool())
	{
		if (!m_GetLocalPlayerIndexHookID)
		{
			m_GetLocalPlayerIndexHookID = Funcs::GetHook_Global_GetLocalPlayerIndex()->AddHook(std::bind(&LocalPlayer::GetLocalPlayerIndexOverride, this));
		}
	}
	else
	{
		if (m_GetLocalPlayerIndexHookID && Funcs::GetHook_Global_GetLocalPlayerIndex()->RemoveHook(m_GetLocalPlayerIndexHookID, __FUNCSIG__))
			m_GetLocalPlayerIndexHookID = 0;
	}
}

void LocalPlayer::SetToCurrentTarget()
{
	Player* localPlayer = Player::GetPlayer(Interfaces::GetEngineClient()->GetLocalPlayer(), __FUNCSIG__);

	if (localPlayer)
	{
		if (localPlayer->GetObserverMode() == OBS_MODE_FIXED ||
			localPlayer->GetObserverMode() == OBS_MODE_IN_EYE ||
			localPlayer->GetObserverMode() == OBS_MODE_CHASE)
		{
			Player* targetPlayer = Player::AsPlayer(localPlayer->GetObserverTarget());

			if (targetPlayer)
			{
				player->SetValue(targetPlayer->GetEntity()->entindex());
				return;
			}
		}
	}

	player->SetValue(Interfaces::GetEngineClient()->GetLocalPlayer());
}

void LocalPlayer::TickPanel::OnTick()
{
	if (Interfaces::GetEngineClient()->IsInGame())
		m_SetToCurrentTarget();
}