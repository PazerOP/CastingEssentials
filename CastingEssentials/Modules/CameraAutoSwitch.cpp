/*
*  cameraautoswitch.cpp
*  StatusSpec project
*
*  Copyright (c) 2014-2015 Forward Command Post
*  BSD 2-Clause License
*  http://opensource.org/licenses/BSD-2-Clause
*
*/

#include "CameraAutoSwitch.h"

#include <client/c_baseentity.h>
#include <cdll_int.h>
#include <gameeventdefs.h>
#include <shareddefs.h>
#include <toolframework/ienginetool.h>
#include <vgui/IVGui.h>

#include "PluginBase/funcs.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/player.h"

#include "PluginBase/StubPanel.h"

class CameraAutoSwitch::Panel : public vgui::StubPanel
{
public:
	virtual void OnTick() override;

	void SwitchToKiller(int player, float delay);
private:
	bool killerSwitch;
	int killerSwitchPlayer;
	float killerSwitchTime;
};

CameraAutoSwitch::CameraAutoSwitch()
{
	panel = nullptr;

	enabled = new ConVar("ce_cameraautoswitch_enabled", "0", FCVAR_NONE, "enable automatic switching of camera", [](IConVar *var, const char *pOldValue, float flOldValue) { Modules().GetModule<CameraAutoSwitch>()->ToggleEnabled(var, pOldValue, flOldValue); });
	m_SwitchToKiller = new ConVar("ce_cameraautoswitch_killer", "0", FCVAR_NONE, "switch to killer upon spectated player death", [](IConVar *var, const char *pOldValue, float flOldValue) { Modules().GetModule<CameraAutoSwitch>()->ToggleKillerEnabled(var, pOldValue, flOldValue); });
	killer_delay = new ConVar("ce_cameraautoswitch_killer_delay", "0", FCVAR_NONE, "delay before switching to killer");
}
CameraAutoSwitch::~CameraAutoSwitch()
{
	Interfaces::GetGameEventManager()->RemoveListener(this);
}

bool CameraAutoSwitch::CheckDependencies()
{
	bool ready = true;

	if (!Interfaces::GetEngineClient())
	{
		PluginWarning("Required interface IVEngineClient for module %s not available!\n", Modules().GetModuleName<CameraAutoSwitch>().c_str());
		ready = false;
	}

	if (!Interfaces::GetEngineTool())
	{
		PluginWarning("Required interface IEngineTool for module %s not available!\n", Modules().GetModuleName<CameraAutoSwitch>().c_str());
		ready = false;
	}

	if (!Interfaces::GetGameEventManager())
	{
		PluginWarning("Required interface IGameEventManager2 for module %s not available!\n", Modules().GetModuleName<CameraAutoSwitch>().c_str());
		ready = false;
	}

	try
	{
		Funcs::GetFunc_C_HLTVCamera_SetPrimaryTarget();
	}
	catch (bad_pointer)
	{
		PluginWarning("Required function C_HLTVCamera::SetPrimaryTarget for module %s not available!\n", Modules().GetModuleName<CameraAutoSwitch>().c_str());
		ready = false;
	}

	if (!Player::CheckDependencies())
	{
		PluginWarning("Required player helper class for module %s not available!\n", Modules().GetModuleName<CameraAutoSwitch>().c_str());
		ready = false;
	}

	try
	{
		Interfaces::GetHLTVCamera();
	}
	catch (bad_pointer)
	{
		PluginWarning("Module %s requires C_HLTVCamera, which cannot be verified at this time!\n", Modules().GetModuleName<CameraAutoSwitch>().c_str());
	}

	return ready;
}

void CameraAutoSwitch::FireGameEvent(IGameEvent *event)
{
	if (enabled->GetBool() && m_SwitchToKiller->GetBool() && !strcmp(event->GetName(), GAME_EVENT_PLAYER_DEATH))
	{
		Player* localPlayer = Player::GetPlayer(Interfaces::GetEngineClient()->GetLocalPlayer(), __FUNCSIG__);
		if (localPlayer)
		{
			if (localPlayer->GetObserverMode() == OBS_MODE_FIXED || localPlayer->GetObserverMode() == OBS_MODE_IN_EYE || localPlayer->GetObserverMode() == OBS_MODE_CHASE)
			{
				Player* targetPlayer = Player::AsPlayer(localPlayer->GetObserverTarget());

				if (targetPlayer)
				{
					if (Interfaces::GetEngineClient()->GetPlayerForUserID(event->GetInt("userid")) == targetPlayer->GetEntity()->entindex())
					{
						Player* killer = Player::GetPlayer(Interfaces::GetEngineClient()->GetPlayerForUserID(event->GetInt("attacker")), __FUNCSIG__);
						if (killer)
						{
							if (killer_delay->GetFloat() > 0.0f)
							{
								if (panel)
									panel->SwitchToKiller(killer->GetEntity()->entindex(), killer_delay->GetFloat());
							}
							else
							{
								try
								{
									Funcs::GetFunc_C_HLTVCamera_SetPrimaryTarget()(killer->GetEntity()->entindex());
								}
								catch (bad_pointer &e)
								{
									Warning("%s\n", e.what());
								}
							}
						}
					}
				}
			}
		}
	}
}

void CameraAutoSwitch::ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue)
{
	if (enabled->GetBool())
	{
		if (!panel)
			panel.reset(new Panel());
	}
	else if (panel)
		panel.reset();
}

void CameraAutoSwitch::ToggleKillerEnabled(IConVar *var, const char *pOldValue, float flOldValue)
{
	if (enabled->GetBool())
	{
		if (!Interfaces::GetGameEventManager()->FindListener(this, GAME_EVENT_PLAYER_DEATH))
		{
			Interfaces::GetGameEventManager()->AddListener(this, GAME_EVENT_PLAYER_DEATH, false);
		}
	}
	else
	{
		if (Interfaces::GetGameEventManager()->FindListener(this, GAME_EVENT_PLAYER_DEATH))
		{
			Interfaces::GetGameEventManager()->RemoveListener(this);
		}
	}
}

void CameraAutoSwitch::Panel::OnTick()
{
	if (killerSwitch && Interfaces::GetEngineTool()->HostTime() > killerSwitchTime)
	{
		killerSwitch = false;

		try
		{
			Funcs::GetFunc_C_HLTVCamera_SetPrimaryTarget()(killerSwitchPlayer);
		}
		catch (bad_pointer &e)
		{
			Warning("%s\n", e.what());
		}
	}
}

void CameraAutoSwitch::Panel::SwitchToKiller(int player, float delay)
{
	killerSwitch = true;
	killerSwitchPlayer = player;
	killerSwitchTime = Interfaces::GetEngineTool()->HostTime() + delay;
}