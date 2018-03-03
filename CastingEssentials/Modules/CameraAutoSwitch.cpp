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

#include "Controls/StubPanel.h"
#include "Misc/HLTVCameraHack.h"
#include "Modules/CameraState.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/player.h"

#include <client/c_baseentity.h>
#include <cdll_int.h>
#include <gameeventdefs.h>
#include <shareddefs.h>
#include <toolframework/ienginetool.h>
#include <vgui/IVGui.h>
#include <vprof.h>

CameraAutoSwitch::CameraAutoSwitch() :
	ce_cameraautoswitch_enabled("ce_cameraautoswitch_enabled", "0", FCVAR_NONE, "enable automatic switching of camera"),

	ce_cameraautoswitch_killer("ce_cameraautoswitch_killer", "0", FCVAR_NONE, "switch to killer upon spectated player death",
		[](IConVar *var, const char *pOldValue, float flOldValue) { GetModule()->ToggleKillerEnabled(var, pOldValue, flOldValue); }),
	ce_cameraautoswitch_killer_delay("ce_cameraautoswitch_killer_delay", "0", FCVAR_NONE, "delay before switching to killer", true, 0, false, FLT_MAX)
{
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
		PluginWarning("Required interface IVEngineClient for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Interfaces::GetEngineTool())
	{
		PluginWarning("Required interface IEngineTool for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Interfaces::GetGameEventManager())
	{
		PluginWarning("Required interface IGameEventManager2 for module %s not available!\n", GetModuleName());
		ready = false;
	}

	try
	{
		HookManager::GetRawFunc<HookFunc::C_HLTVCamera_SetPrimaryTarget>();
	}
	catch (bad_pointer)
	{
		PluginWarning("Required function C_HLTVCamera::SetPrimaryTarget for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Player::CheckDependencies())
	{
		PluginWarning("Required player helper class for module %s not available!\n", GetModuleName());
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

void CameraAutoSwitch::FireGameEvent(IGameEvent *event)
{
	if (!strcmp(event->GetName(), GAME_EVENT_PLAYER_DEATH))
		OnPlayerDeath(event);
}

void CameraAutoSwitch::OnPlayerDeath(IGameEvent* event)
{
	if (!ce_cameraautoswitch_enabled.GetBool() || !ce_cameraautoswitch_killer.GetBool())
		return;

	Assert(!strcmp(event->GetName(), GAME_EVENT_PLAYER_DEATH));

	// Ignore feign death
	const auto deathFlags = event->GetInt("death_flags");
	if (deathFlags & 0x32)
		return;

	const auto targetUserID = Interfaces::GetEngineClient()->GetPlayerForUserID(event->GetInt("userid"));
	if (!Player::IsValidIndex(targetUserID))
		return;

	Player* const localPlayer = Player::GetLocalPlayer();
	if (!localPlayer)
		return;

	if (const auto cameraState = CameraState::GetModule(); cameraState)
	{
		if (cameraState->GetObserverMode() == OBS_MODE_ROAMING)
			return;
	}

	Player* const targetPlayer = Player::AsPlayer(localPlayer->GetObserverTarget());
	if (!targetPlayer)
		return;

	// We only switch when our currently spectated player dies, don't care about other players dying
	if (targetUserID != targetPlayer->GetEntity()->entindex())
		return;

	Player* const killer = Player::GetPlayer(Interfaces::GetEngineClient()->GetPlayerForUserID(event->GetInt("attacker")), __FUNCSIG__);
	if (!killer)
		return;

	if (ce_cameraautoswitch_killer_delay.GetFloat() > 0.0f)
	{
		// Switch with a delay
		QueueSwitchToPlayer(killer->entindex(), targetPlayer->entindex(), ce_cameraautoswitch_killer_delay.GetFloat());
	}
	else
	{
		// Switch immediately
		try
		{
			Interfaces::GetHLTVCamera()->SetPrimaryTarget(killer->GetEntity()->entindex());
		}
		catch (bad_pointer &e)
		{
			Warning("%s\n", e.what());
		}
	}
}

void CameraAutoSwitch::ToggleKillerEnabled(IConVar *var, const char *pOldValue, float flOldValue)
{
	if (ce_cameraautoswitch_enabled.GetBool())
	{
		if (!Interfaces::GetGameEventManager()->FindListener(this, GAME_EVENT_PLAYER_DEATH))
			Interfaces::GetGameEventManager()->AddListener(this, GAME_EVENT_PLAYER_DEATH, false);
	}
	else
	{
		if (Interfaces::GetGameEventManager()->FindListener(this, GAME_EVENT_PLAYER_DEATH))
			Interfaces::GetGameEventManager()->RemoveListener(this);
	}
}

void CameraAutoSwitch::OnTick(bool inGame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (!inGame)
	{
		m_AutoSwitchQueued = false;
		return;
	}

	if (ce_cameraautoswitch_enabled.GetBool() && m_AutoSwitchQueued && Interfaces::GetEngineTool()->HostTime() >= m_AutoSwitchTime)
	{
		m_AutoSwitchQueued = false;
		Player* const localObserverTarget = Player::GetLocalObserverTarget();
		if (localObserverTarget)
		{
			const int localObserverTargetIndex = localObserverTarget->entindex();
			if (m_AutoSwitchFromPlayer == localObserverTargetIndex)
			{
				try
				{
					Interfaces::GetHLTVCamera()->SetPrimaryTarget(m_AutoSwitchToPlayer);
				}
				catch (bad_pointer &e)
				{
					Warning("%s\n", e.what());
				}
			}
		}
	}
}

void CameraAutoSwitch::QueueSwitchToPlayer(int toPlayer, int fromPlayer, float delay)
{
	m_AutoSwitchQueued = true;
	m_AutoSwitchToPlayer = toPlayer;
	m_AutoSwitchFromPlayer = fromPlayer;
	m_AutoSwitchTime = Interfaces::GetEngineTool()->HostTime() + delay;
}