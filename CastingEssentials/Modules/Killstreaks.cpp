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

#include "PluginBase/entities.h"
#include "PluginBase/funcs.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/player.h"
#include "PluginBase/TFDefinitions.h"

#include "PluginBase/StubPanel.h"

class Killstreaks::Panel final : public vgui::StubPanel {
public:
	Panel();
	virtual ~Panel();

	virtual void OnTick();

private:
	bool FireEventClientSideOverride(IGameEvent *event);

	int bluTopKillstreak;
	int bluTopKillstreakPlayer;
	std::map<int, int> currentKillstreaks;
	int fireEventClientSideHook;
	CHandle<C_BaseEntity> gameResourcesEntity;
	int redTopKillstreak;
	int redTopKillstreakPlayer;
};

Killstreaks::Killstreaks()
{
	enabled = new ConVar("ce_killstreaks_enabled", "0", FCVAR_NONE, "enable killstreaks display", [](IConVar *var, const char *pOldValue, float flOldValue) { GetModule()->ToggleEnabled(var, pOldValue, flOldValue); });
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
		char index[4];
		PropIndex(i, index);

		if (!Entities::RetrieveClassPropOffset("CTFPlayer", { "m_hMyWeapons", index }))
		{
			PluginWarning("Required property table m_hMyWeapons for CTFPlayer for module %s not available!\n", GetModuleName());

			ready = false;

			break;
		}
	}

	for (int i = 0; i < 3; i++)
	{
		char index[4];
		PropIndex(i, index);

		if (!Entities::RetrieveClassPropOffset("CTFPlayer", { "m_nStreaks", index }))
		{
			PluginWarning("Required property table m_nStreaks for CTFPlayer for module %s not available!\n", GetModuleName());
			ready = false;
			break;
		}
	}

	for (int i = 0; i <= MAX_PLAYERS; i++)
	{
		char index[4];
		PropIndex(i, index);

		if (!Entities::RetrieveClassPropOffset("CTFPlayerResource", { "m_iStreaks", index }))
		{
			PluginWarning("Required property table m_iStreaks for CTFPlayerResource for module %s not available!\n", GetModuleName());
			ready = false;
			break;
		}
	}

	if (!Entities::RetrieveClassPropOffset("CWeaponMedigun", { "m_bHealing" }))
	{
		PluginWarning("Required property m_bHealing for CWeaponMedigun for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Entities::RetrieveClassPropOffset("CWeaponMedigun", { "m_hHealingTarget" }))
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
			panel.reset(new Panel());
	}
	else if (panel)
		panel.reset();
}

Killstreaks::Panel::Panel()
{
	bluTopKillstreak = 0;
	bluTopKillstreakPlayer = 0;

	auto test = Interfaces::GetGameEventManager();
	fireEventClientSideHook = Funcs::GetHook_IGameEventManager2_FireEventClientSide()->AddHook(
		std::bind(&Killstreaks::Panel::FireEventClientSideOverride, this, std::placeholders::_1));
	redTopKillstreak = 0;
	redTopKillstreakPlayer = 0;
}

Killstreaks::Panel::~Panel()
{
	Funcs::GetHook_IGameEventManager2_FireEventClientSide()->RemoveHook(fireEventClientSideHook, __FUNCSIG__);

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
				char index[4];
				PropIndex(i, index);

				int *killstreakGlobal = Entities::GetEntityProp<int *>(entity, { "m_iStreaks", index });
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

		int *killstreakPrimary = Entities::GetEntityProp<int *>(playerEntity, { "m_nStreaks", "000" });
		int *killstreakSecondary = Entities::GetEntityProp<int *>(playerEntity, { "m_nStreaks", "001" });
		int *killstreakMelee = Entities::GetEntityProp<int *>(playerEntity, { "m_nStreaks", "002" });
		int *killstreakPDA = Entities::GetEntityProp<int *>(playerEntity, { "m_nStreaks", "003" });

		*killstreakPrimary = 0;
		*killstreakSecondary = 0;
		*killstreakMelee = 0;
		*killstreakPDA = 0;
	}
}

void Killstreaks::Panel::OnTick()
{
	if (Interfaces::GetEngineClient()->IsConnected())
	{
		if (!gameResourcesEntity.IsValid() || !gameResourcesEntity.Get())
		{
			int maxEntity = Interfaces::GetClientEntityList()->GetHighestEntityIndex();

			for (int i = 1; i <= maxEntity; i++)
			{
				IClientEntity *entity = Interfaces::GetClientEntityList()->GetClientEntity(i);

				if (!entity)
					continue;

				if (Entities::CheckEntityBaseclass(entity, "TFPlayerResource"))
				{
					gameResourcesEntity = dynamic_cast<C_BaseEntity *>(entity);
					break;
				}
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

			int *killstreakPrimary = Entities::GetEntityProp<int *>(playerEntity, { "m_nStreaks", "000" });
			int *killstreakSecondary = Entities::GetEntityProp<int *>(playerEntity, { "m_nStreaks", "001" });
			int *killstreakMelee = Entities::GetEntityProp<int *>(playerEntity, { "m_nStreaks", "002" });
			int *killstreakPDA = Entities::GetEntityProp<int *>(playerEntity, { "m_nStreaks", "003" });

			int userid = player->GetUserID();

			if (currentKillstreaks.find(userid) != currentKillstreaks.end())
			{
				*killstreakPrimary = currentKillstreaks[userid];
				*killstreakSecondary = currentKillstreaks[userid];
				*killstreakMelee = currentKillstreaks[userid];
				*killstreakPDA = currentKillstreaks[userid];

				if (gameResourcesEntity.IsValid() && gameResourcesEntity.Get())
				{
					char index[4];
					PropIndex(playerEntity->entindex(), index);

					int *killstreakGlobal = Entities::GetEntityProp<int *>(gameResourcesEntity.Get(), { "m_iStreaks", index });
					*killstreakGlobal = currentKillstreaks[userid];
				}
			}
			else
			{
				*killstreakPrimary = 0;
				*killstreakSecondary = 0;
				*killstreakMelee = 0;
				*killstreakPDA = 0;

				if (gameResourcesEntity.IsValid() && gameResourcesEntity.Get())
				{
					char index[4];
					PropIndex(playerEntity->entindex(), index);

					int *killstreakGlobal = Entities::GetEntityProp<int *>(gameResourcesEntity.Get(), { "m_iStreaks", index });
					*killstreakGlobal = 0;
				}
			}
		}
	}
	else
	{
		currentKillstreaks.clear();
	}
}

bool Killstreaks::Panel::FireEventClientSideOverride(IGameEvent *event)
{
	Funcs::GetHook_IGameEventManager2_FireEventClientSide()->SetState(HookAction::SUPERCEDE);
	return Funcs::GetHook_IGameEventManager2_FireEventClientSide()->GetOriginal()(event);

	const char* eventName = event->GetName();
	if (!strcmp(eventName, "player_spawn"))
	{
		int userID = event->GetInt("userid", -1);

		if (userID != -1)
			currentKillstreaks.erase(userID);
	}
	else if (!strcmp(eventName, "player_death"))
	{
		const int victimUserID = event->GetInt("userid", -1);
		const int attackerUserID = event->GetInt("attacker", -1);
		if (attackerUserID != -1)
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
		if (assisterUserID != -1)
		{
			Player* assister = Player::GetPlayer(Interfaces::GetEngineClient()->GetPlayerForUserID(assisterUserID), __FUNCSIG__);
			if (assister)
			{
				for (int i = 0; i < MAX_WEAPONS; i++)
				{
					char index[4];
					PropIndex(i, index);

					IClientEntity *weapon = Entities::GetEntityProp<EHANDLE *>(assister->GetEntity(), { "m_hMyWeapons", index })->Get();
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

		if (victimUserID != -1)
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

	Funcs::GetHook_IGameEventManager2_FireEventClientSide()->SetState(HookAction::IGNORE);
	return true;
}