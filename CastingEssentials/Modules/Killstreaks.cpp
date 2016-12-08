/*
*  killstreaks.cpp
*  StatusSpec project
*
*  Copyright (c) 2014-2015 Forward Command Post
*  BSD 2-Clause License
*  http://opensource.org/licenses/BSD-2-Clause
*
*/

#include "killstreaks.h"

#include <client/c_baseentity.h>
#include <convar.h>
#include <igameevents.h>
#include <shareddefs.h>
#include <vprof.h>

#include <PolyHook.h>

#include "PluginBase/entities.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/player.h"
#include "PluginBase/TFDefinitions.h"
#include "PluginBase/TFPlayerResource.h"

#include "Controls/StubPanel.h"

class Killstreaks::Panel final : public vgui::StubPanel {
public:
	Panel();
	void Init();
	virtual ~Panel();

	virtual void OnTick();

private:
	bool FireEventClientSideOverride(IGameEvent *event);

	std::unique_ptr<PLH::VFuncSwap> m_Detour;

	static bool __fastcall DetourFnTest(IGameEventManager2*, void*, IGameEvent*);

	int bluTopKillstreak;
	int bluTopKillstreakPlayer;
	std::map<int, int> currentKillstreaks;
	int m_FireEventClientSideHook;
	int redTopKillstreak;
	int redTopKillstreakPlayer;
};

Killstreaks::Killstreaks()
{
	enabled = new ConVar("ce_killstreaks_enabled", "0", FCVAR_NONE, "enable killstreaks display");
	panel.reset(new Panel());
}

bool Killstreaks::CheckDependencies()
{
	bool ready = true;

	if (!Interfaces::GetClientEntityList())
	{
		PluginWarning("Required interface IClientEntityList for module %s not available!\n", GetModuleName());

		ready = false;
	}

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

	if (!Interfaces::GetGameEventManager())
	{
		PluginWarning("Required interface IGameEventManager2 for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Player::CheckDependencies())
	{
		PluginWarning("Required player helper class for module %s not available!\n", GetModuleName());
		ready = false;
	}

	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		char buffer[32];
		Entities::PropIndex(buffer, "m_hMyWeapons", i);

		if (Entities::RetrieveClassPropOffset("CTFPlayer", buffer) < 0)
		{
			PluginWarning("Required property table m_hMyWeapons for CTFPlayer for module %s not available!\n", GetModuleName());

			ready = false;

			break;
		}
	}

	for (int i = 0; i < 3; i++)
	{
		char buffer[32];
		Entities::PropIndex(buffer, "m_nStreaks", i);

		if (Entities::RetrieveClassPropOffset("CTFPlayer", buffer) < 0)
		{
			PluginWarning("Required property table %s for CTFPlayer for module %s not available!\n", buffer, GetModuleName());
			ready = false;
			break;
		}
	}

	for (int i = 0; i <= MAX_PLAYERS; i++)
	{
		char buffer[32];
		Entities::PropIndex(buffer, "m_iStreaks", i);

		if (Entities::RetrieveClassPropOffset("CTFPlayerResource", buffer) < 0)
		{
			PluginWarning("Required property table %s for CTFPlayerResource for module %s not available!\n", buffer, GetModuleName());
			ready = false;
			break;
		}
	}

	if (Entities::RetrieveClassPropOffset("CWeaponMedigun", "m_bHealing") < 0)
	{
		PluginWarning("Required property m_bHealing for CWeaponMedigun for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (Entities::RetrieveClassPropOffset("CWeaponMedigun", "m_hHealingTarget") < 0)
	{
		PluginWarning("Required property m_hHealingTarget for CWeaponMedigun for module %s not available!\n", GetModuleName());
		ready = false;
	}

	return ready;
}

void Killstreaks::ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue)
{
	if (enabled->GetBool())
	{
		if (!panel)
		{
			panel.reset(new Panel());
			panel->Init();
		}
	}
	else if (panel)
		panel.reset();
}

Killstreaks::Panel::Panel()
{
	m_FireEventClientSideHook = 0;
	bluTopKillstreak = 0;
	bluTopKillstreakPlayer = 0;
	redTopKillstreak = 0;
	redTopKillstreakPlayer = 0;
}

void Killstreaks::Panel::Init()
{
	m_FireEventClientSideHook = GetHooks()->AddHook<IGameEventManager2_FireEventClientSide>(std::bind(&Killstreaks::Panel::FireEventClientSideOverride, this, std::placeholders::_1));
}

Killstreaks::Panel::~Panel()
{
	if (m_FireEventClientSideHook && GetHooks()->RemoveHook<IGameEventManager2_FireEventClientSide>(m_FireEventClientSideHook, __FUNCSIG__))
		m_FireEventClientSideHook = 0;

	Assert(!m_FireEventClientSideHook);

	int maxEntity = Interfaces::GetClientEntityList()->GetHighestEntityIndex();

	for (int e = 1; e <= maxEntity; e++)
	{
		IClientEntity *entity = Interfaces::GetClientEntityList()->GetClientEntity(e);
		if (!entity)
			continue;

		if (Entities::CheckEntityBaseclass(entity, "TFPlayerResource"))
		{
			for (int i = 1; i <= MAX_PLAYERS; i++)
			{
				char buffer[32];
				Entities::PropIndex(buffer, "m_iStreaks", i);

				int *killstreakGlobal = Entities::GetEntityProp<int *>(entity, buffer);
				*killstreakGlobal = 0;
			}

			break;
		}
	}

	for (Player* player : Player::Iterable())
	{
		Assert(player);
		if (!player)
			continue;

		auto playerEntity = player->GetEntity();
		Assert(playerEntity);
		if (!playerEntity)
			continue;

		int *killstreakPrimary = Entities::GetEntityProp<int *>(playerEntity, "m_nStreaks.000");
		int *killstreakSecondary = Entities::GetEntityProp<int *>(playerEntity, "m_nStreaks.001");
		int *killstreakMelee = Entities::GetEntityProp<int *>(playerEntity, "m_nStreaks.002");
		int *killstreakPDA = Entities::GetEntityProp<int *>(playerEntity, "m_nStreaks.003");

		*killstreakPrimary = 0;
		*killstreakSecondary = 0;
		*killstreakMelee = 0;
		*killstreakPDA = 0;
	}
}

void Killstreaks::Panel::OnTick()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (Interfaces::GetEngineClient()->IsInGame())
	{
		if (!m_FireEventClientSideHook && GetHooks()->GetHook<IGameEventManager2_FireEventClientSide>())
		{
			m_FireEventClientSideHook = GetHooks()->AddHook<IGameEventManager2_FireEventClientSide>(std::bind(&Killstreaks::Panel::FireEventClientSideOverride, this, std::placeholders::_1));
		}

		Assert(TFPlayerResource::GetPlayerResource());
		if (!TFPlayerResource::GetPlayerResource())
		{
			Warning("%s: TFPlayerResource unavailable!\n", Killstreaks::GetModuleName());
			return;
		}

		for (Player* player : Player::Iterable())
		{
			Assert(player);
			if (!player)
				continue;

			auto playerEntity = player->GetEntity();
			Assert(playerEntity);
			if (!playerEntity)
				continue;

			int* const killstreakPrimary = Entities::GetEntityProp<int *>(playerEntity, "m_nStreaks.000");
			int* const killstreakSecondary = Entities::GetEntityProp<int *>(playerEntity, "m_nStreaks.001");
			int* const killstreakMelee = Entities::GetEntityProp<int *>(playerEntity, "m_nStreaks.002");
			int* const killstreakPDA = Entities::GetEntityProp<int *>(playerEntity, "m_nStreaks.003");

			const int userid = player->GetUserID();
			int* const killstreakGlobal = TFPlayerResource::GetPlayerResource()->GetKillstreak(player->entindex());

			if (currentKillstreaks.find(userid) != currentKillstreaks.end())
			{
				*killstreakPrimary = currentKillstreaks[userid];
				*killstreakSecondary = currentKillstreaks[userid];
				*killstreakMelee = currentKillstreaks[userid];
				*killstreakPDA = currentKillstreaks[userid];
				*killstreakGlobal = currentKillstreaks[userid];
			}
			else
			{
				*killstreakPrimary = 0;
				*killstreakSecondary = 0;
				*killstreakMelee = 0;
				*killstreakPDA = 0;
				*killstreakGlobal = 0;
			}
		}
	}
	else
	{
		currentKillstreaks.clear();

		if (m_FireEventClientSideHook)
		{
			if (GetHooks()->RemoveHook<IGameEventManager2_FireEventClientSide>(m_FireEventClientSideHook, __FUNCSIG__))
				m_FireEventClientSideHook = 0;

			Assert(!m_FireEventClientSideHook);
		}
	}
}

bool Killstreaks::Panel::FireEventClientSideOverride(IGameEvent *event)
{
	Assert(event);
	if (!event)
		return false;

	const char* eventName = event->GetName();
	if (!strcmp(eventName, "player_spawn"))
	{
		int userID = event->GetInt("userid", -1);

		if (userID > 0)
			currentKillstreaks.erase(userID);
	}
	else if (!strcmp(eventName, "player_death"))
	{
		const int victimUserID = event->GetInt("userid", -1);
		const int attackerUserID = event->GetInt("attacker", -1);
		if (attackerUserID > 0)
		{
			if (attackerUserID != victimUserID)
			{
				currentKillstreaks[attackerUserID]++;

				event->SetInt("kill_streak_total", currentKillstreaks[attackerUserID]);
				event->SetInt("kill_streak_wep", currentKillstreaks[attackerUserID]);

				Player* attacker = Player::GetPlayer(Interfaces::GetEngineClient()->GetPlayerForUserID(attackerUserID), __FUNCSIG__);

				if (attacker)
				{
					if (attacker->GetTeam() == TFTeam::Red)
					{
						if (currentKillstreaks[attackerUserID] > redTopKillstreak)
						{
							redTopKillstreak = currentKillstreaks[attackerUserID];
							redTopKillstreakPlayer = attackerUserID;
						}
					}
					else if (attacker->GetTeam() == TFTeam::Blue)
					{
						if (currentKillstreaks[attackerUserID] > bluTopKillstreak)
						{
							bluTopKillstreak = currentKillstreaks[attackerUserID];
							bluTopKillstreakPlayer = attackerUserID;
						}
					}
				}
			}
			else
			{
				event->SetInt("kill_streak_total", 0);
				event->SetInt("kill_streak_wep", 0);
			}
		}

		const int assisterUserID = event->GetInt("assister", -1);
		if (assisterUserID > 0)
		{
			Player* assister = Player::GetPlayer(Interfaces::GetEngineClient()->GetPlayerForUserID(assisterUserID), __FUNCSIG__);
			if (assister)
			{
				for (int i = 0; i < MAX_WEAPONS; i++)
				{
					char buffer[32];
					Entities::PropIndex(buffer, "m_hMyWeapons", i);

					IClientEntity *weapon = Entities::GetEntityProp<EHANDLE *>(assister->GetEntity(), buffer)->Get();
					if (!weapon || !Entities::CheckEntityBaseclass(weapon, "WeaponMedigun"))
						continue;

					bool healing = *Entities::GetEntityProp<bool *>(weapon, { "m_bHealing" });
					if (healing)
					{
						int healingTarget = Entities::GetEntityProp<EHANDLE *>(weapon, { "m_hHealingTarget" })->GetEntryIndex();

						if (healingTarget == Interfaces::GetEngineClient()->GetPlayerForUserID(attackerUserID))
						{
							currentKillstreaks[assisterUserID]++;

							if (assister->GetTeam() == TFTeam::Red)
							{
								if (currentKillstreaks[assisterUserID] > redTopKillstreak)
								{
									redTopKillstreak = currentKillstreaks[assisterUserID];
									redTopKillstreakPlayer = assisterUserID;
								}
							}
							else if (assister->GetTeam() == TFTeam::Blue)
							{
								if (currentKillstreaks[assisterUserID] > bluTopKillstreak)
								{
									bluTopKillstreak = currentKillstreaks[assisterUserID];
									bluTopKillstreakPlayer = assisterUserID;
								}
							}
						}
					}
				}
			}

			event->SetInt("kill_streak_assist", currentKillstreaks[assisterUserID]);
		}

		if (victimUserID > 0)
			event->SetInt("kill_streak_victim", currentKillstreaks[victimUserID]);
	}
	else if (!strcmp(eventName, "teamplay_win_panel"))
	{
		if (event->GetInt("winning_team") == (int)TFTeam::Red)
		{
			event->SetInt("killstreak_player_1", Interfaces::GetEngineClient()->GetPlayerForUserID(redTopKillstreakPlayer));
			event->SetInt("killstreak_player_1_count", redTopKillstreak);
		}
		else if (event->GetInt("winning_team") == (int)TFTeam::Blue)
		{
			event->SetInt("killstreak_player_1", Interfaces::GetEngineClient()->GetPlayerForUserID(bluTopKillstreakPlayer));
			event->SetInt("killstreak_player_1_count", bluTopKillstreak);
		}
	}
	else if (!strcmp(eventName, "teamplay_round_start"))
	{
		bluTopKillstreak = 0;
		bluTopKillstreakPlayer = 0;
		redTopKillstreak = 0;
		redTopKillstreakPlayer = 0;
	}

	return true;
}