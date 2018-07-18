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
#include <igameevents.h>
#include <shareddefs.h>
#include <toolframework/ienginetool.h>
#include <vprof.h>

#include <PolyHook.hpp>

#include "Modules/CameraState.h"

#include "PluginBase/entities.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/player.h"
#include "PluginBase/TFDefinitions.h"
#include "PluginBase/TFPlayerResource.h"

#include "Controls/StubPanel.h"

std::array<EntityOffset<int>, 4> Killstreaks::s_PlayerStreaks;
EntityOffset<bool> Killstreaks::s_MedigunHealing;
EntityOffset<EHANDLE> Killstreaks::s_MedigunHealingTarget;

class Killstreaks::Panel final : public vgui::StubPanel {
public:
	Panel();
	void Init();
	virtual ~Panel();

	virtual void OnTick();

private:
	bool FireEventClientSideOverride(IGameEvent *event);

	int bluTopKillstreak;
	int bluTopKillstreakPlayer;
	std::map<int, int> currentKillstreaks;
	int m_FireEventClientSideHook;
	int redTopKillstreak;
	int redTopKillstreakPlayer;
};

Killstreaks::Killstreaks() :
	ce_killstreaks_enabled("ce_killstreaks_enabled", "0", FCVAR_NONE, "Show killstreak counts on the hud for all weapons, not just killstreak weapons."),

	ce_killstreaks_hide_firstperson_effects("ce_killstreaks_hide_firstperson_effects", "0", FCVAR_NONE,
		"Don't show professional killstreak eye effects in the middle of the screen for the person we're spectating when in firstperson camera mode.")
{
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

	{
		const auto playerClass = Entities::GetClientClass("CTFPlayer");

		for (size_t i = 0; i < s_PlayerStreaks.size(); i++)
		{
			char buf[32];
			s_PlayerStreaks[i] = Entities::GetEntityProp<int>(playerClass,
				Entities::PropIndex(buf, "m_nStreaks", i));
		}

		const auto medigunClass = Entities::GetClientClass("CWeaponMedigun");
		s_MedigunHealing = Entities::GetEntityProp<bool>(medigunClass, "m_bHealing");
		s_MedigunHealingTarget = Entities::GetEntityProp<EHANDLE>(medigunClass, "m_hHealingTarget");
	}

	return ready;
}

void Killstreaks::OnTick(bool inGame)
{
	if (!inGame)
		return;

	if (!ce_killstreaks_hide_firstperson_effects.GetBool())
		return;

	for (Player* player : Player::Iterable())
	{
		const bool shouldHideKS =
			player == Player::AsPlayer(CameraState::GetLocalObserverTarget()) &&
			CameraState::GetLocalObserverMode() == ObserverMode::OBS_MODE_IN_EYE;

		auto entParticles = player->GetBaseEntity()->ParticleProp();
		for (int i = 0; i < entParticles->GetParticleEffectCount(); i++)
		{
			auto effect = entParticles->GetParticleEffectFromIdx(i);

			static constexpr const char KILLSTREAK_PREFIX[] = "killstreak_";
			const char* name = effect->GetEffectName();
			if (strncmp(KILLSTREAK_PREFIX, name, std::size(KILLSTREAK_PREFIX) - 1))
				continue;	// This isn't a killstreak particle effect

			effect->SetDormant(shouldHideKS);
		}
	}
}

void Killstreaks::ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue)
{
	if (ce_killstreaks_enabled.GetBool())
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
	m_FireEventClientSideHook = GetHooks()->AddHook<HookFunc::IGameEventManager2_FireEventClientSide>(std::bind(&Killstreaks::Panel::FireEventClientSideOverride, this, std::placeholders::_1));
}

Killstreaks::Panel::~Panel()
{
	if (m_FireEventClientSideHook && GetHooks()->RemoveHook<HookFunc::IGameEventManager2_FireEventClientSide>(m_FireEventClientSideHook, __FUNCSIG__))
		m_FireEventClientSideHook = 0;

	Assert(!m_FireEventClientSideHook);

	for (int i = 1; i <= Interfaces::GetEngineTool()->GetMaxClients(); i++)
		*TFPlayerResource::GetPlayerResource()->GetKillstreak(i) = 0;

	for (Player* player : Player::Iterable())
	{
		Assert(player);
		if (!player)
			continue;

		auto playerEntity = player->GetEntity();
		Assert(playerEntity);
		if (!playerEntity)
			continue;

		for (auto& streak : s_PlayerStreaks)
			streak.GetValue(playerEntity) = 0;
	}
}

void Killstreaks::Panel::OnTick()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (Interfaces::GetEngineClient()->IsInGame())
	{
		if (!m_FireEventClientSideHook && GetHooks()->GetHook<HookFunc::IGameEventManager2_FireEventClientSide>())
		{
			m_FireEventClientSideHook = GetHooks()->AddHook<HookFunc::IGameEventManager2_FireEventClientSide>(std::bind(&Killstreaks::Panel::FireEventClientSideOverride, this, std::placeholders::_1));
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

			const int userid = player->GetUserID();
			int* const killstreakGlobal = TFPlayerResource::GetPlayerResource()->GetKillstreak(player->entindex());

			if (auto found = currentKillstreaks.find(userid); found != currentKillstreaks.end())
			{
				for (auto& streak : s_PlayerStreaks)
					streak.GetValue(playerEntity) = found->second;

				*killstreakGlobal = currentKillstreaks[userid];
			}
			else
			{
				for (auto& streak : s_PlayerStreaks)
					streak.GetValue(playerEntity) = 0;

				if (killstreakGlobal)
					*killstreakGlobal = 0;
				else
					PluginWarning("Unable to clear out global killstreaks counter for player %i\n", player->entindex());
			}
		}
	}
	else
	{
		currentKillstreaks.clear();

		if (m_FireEventClientSideHook)
		{
			if (GetHooks()->RemoveHook<HookFunc::IGameEventManager2_FireEventClientSide>(m_FireEventClientSideHook, __FUNCSIG__))
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

					IClientEntity *weapon = Entities::GetEntityProp<EHANDLE>(assister->GetEntity(), buffer).GetValue(assister->GetEntity()).Get();
					if (!weapon || !Entities::CheckEntityBaseclass(weapon, "WeaponMedigun"))
						continue;

					if (s_MedigunHealing.GetValue(weapon))
					{
						int healingTarget = s_MedigunHealingTarget.GetValue(weapon).GetEntryIndex();
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