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
#include <client/c_basecombatweapon.h>
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

MODULE_REGISTER(Killstreaks);

std::array<EntityOffset<int>, 4> Killstreaks::s_PlayerStreaks;
EntityOffset<bool> Killstreaks::s_MedigunHealing;
EntityOffset<EHANDLE> Killstreaks::s_MedigunHealingTarget;
EntityTypeChecker Killstreaks::s_MedigunType;

Killstreaks::Killstreaks() :
	ce_killstreaks_enabled("ce_killstreaks_enabled", "0", FCVAR_NONE, "Show killstreak counts on the hud for all weapons, not just killstreak weapons."),
	ce_killstreaks_debug("ce_killstreaks_debug", "0"),

	ce_killstreaks_hide_firstperson_effects("ce_killstreaks_hide_firstperson_effects", "0", FCVAR_NONE,
		"Don't show professional killstreak eye effects in the middle of the screen for the person we're spectating when in firstperson camera mode."),

	m_FireEventClientSideHook(std::bind(&Killstreaks::FireEventClientSideOverride, this, std::placeholders::_1))
{
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
		s_MedigunType = Entities::GetTypeChecker(medigunClass);
	}

	return ready;
}

void Killstreaks::OnTick(bool inGame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);

	if (ce_killstreaks_enabled.GetBool())
		UpdateKillstreaks(inGame);

	if (inGame && ce_killstreaks_hide_firstperson_effects.GetBool())
		HideFirstPersonEffects();
}

void Killstreaks::HideFirstPersonEffects() const
{
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

void Killstreaks::UpdateKillstreaks(bool inGame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (inGame)
	{
		m_FireEventClientSideHook.Enable();

		Assert(TFPlayerResource::GetPlayerResource());
		if (!TFPlayerResource::GetPlayerResource())
		{
			Warning("%s: TFPlayerResource unavailable!\n", Killstreaks::GetModuleName());
			return;
		}

		int printIndex = 0;

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
			const auto entindex = player->entindex();

			int newStreak = 0;
			if (auto found = m_CurrentKillstreaks.find(userid); found != m_CurrentKillstreaks.end())
				newStreak = found->second;

			if (ce_killstreaks_debug.GetBool())
				engine->Con_NPrintf(printIndex++, "%s: %i", player->GetName(), newStreak);

			for (auto& streak : s_PlayerStreaks)
				streak.GetValue(playerEntity) = newStreak;

			for (int i = 0; i < TFPlayerResource::STREAK_WEAPONS; i++)
			{
				auto streak = TFPlayerResource::GetPlayerResource()->GetKillstreak(entindex, i);
				if (streak)
					*streak = newStreak;
			}
		}
	}
	else
	{
		m_CurrentKillstreaks.clear();
		m_FireEventClientSideHook.Disable();
	}
}

bool Killstreaks::FireEventClientSideOverride(IGameEvent *event)
{
	Assert(event);
	if (!event)
		return false;

	static constexpr Color DBG_COLOR(89, 191, 45, 255);

	const char* eventName = event->GetName();
	if (!strcmp(eventName, "player_spawn"))
	{
		int userID = event->GetInt("userid", -1);

		if (userID > 0)
			m_CurrentKillstreaks.erase(userID);
	}
	else if (!strcmp(eventName, "player_death"))
	{
		const int victimUserID = event->GetInt("userid", -1);
		const int attackerUserID = event->GetInt("attacker", -1);
		if (attackerUserID > 0)
		{
			if (attackerUserID != victimUserID)
			{
				const auto killstreak = ++m_CurrentKillstreaks[attackerUserID];

				event->SetInt("kill_streak_total", killstreak);
				event->SetInt("kill_streak_wep", killstreak);

				Player* attacker = Player::GetPlayerFromUserID(attackerUserID);

				if (ce_killstreaks_debug.GetBool())
				{
					const char* victimName = "(null)";
					if (auto victim = Player::GetPlayerFromUserID(victimUserID))
						victimName = victim->GetName();

					ConColorMsg(DBG_COLOR, "[ce_killstreaks_debug]: %s killed %s (killstreak %i)\n", attacker->GetName(), victimName, killstreak);
				}

				if (attacker)
				{
					if (attacker->GetTeam() == TFTeam::Red)
					{
						if (killstreak > m_RedTopKillstreak)
						{
							m_RedTopKillstreak = killstreak;
							m_RedTopKillstreakPlayer = attackerUserID;
						}
					}
					else if (attacker->GetTeam() == TFTeam::Blue)
					{
						if (killstreak > m_BluTopKillstreak)
						{
							m_BluTopKillstreak = killstreak;
							m_BluTopKillstreakPlayer = attackerUserID;
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
			if (Player* assister = Player::GetPlayerFromUserID(assisterUserID))
			{
				if (auto medigun = assister->GetMedigun())
				{
					if (s_MedigunHealing.GetValue(medigun))
					{
						int healingTarget = s_MedigunHealingTarget.GetValue(medigun).GetEntryIndex();
						if (healingTarget == Interfaces::GetEngineClient()->GetPlayerForUserID(attackerUserID))
						{
							const auto killstreak = ++m_CurrentKillstreaks[assisterUserID];

							if (ce_killstreaks_debug.GetBool())
							{
								const char* victimName = "(null)";
								if (auto victim = Player::GetPlayerFromUserID(victimUserID))
									victimName = victim->GetName();

								ConColorMsg(DBG_COLOR, "[ce_killstreaks_debug]: %s assisted against %s (killstreak %i)\n", assister->GetName(), victimName);
							}

							if (assister->GetTeam() == TFTeam::Red)
							{
								if (killstreak > m_RedTopKillstreak)
								{
									m_RedTopKillstreak = killstreak;
									m_RedTopKillstreakPlayer = assisterUserID;
								}
							}
							else if (assister->GetTeam() == TFTeam::Blue)
							{
								if (killstreak > m_BluTopKillstreak)
								{
									m_BluTopKillstreak = killstreak;
									m_BluTopKillstreakPlayer = assisterUserID;
								}
							}
						}
					}
				}
			}

			event->SetInt("kill_streak_assist", m_CurrentKillstreaks[assisterUserID]);
		}

		if (victimUserID > 0)
			event->SetInt("kill_streak_victim", m_CurrentKillstreaks[victimUserID]);
	}
	else if (!strcmp(eventName, "teamplay_win_panel"))
	{
		if (event->GetInt("winning_team") == (int)TFTeam::Red)
		{
			event->SetInt("killstreak_player_1", Interfaces::GetEngineClient()->GetPlayerForUserID(m_RedTopKillstreakPlayer));
			event->SetInt("killstreak_player_1_count", m_RedTopKillstreak);
		}
		else if (event->GetInt("winning_team") == (int)TFTeam::Blue)
		{
			event->SetInt("killstreak_player_1", Interfaces::GetEngineClient()->GetPlayerForUserID(m_BluTopKillstreakPlayer));
			event->SetInt("killstreak_player_1_count", m_BluTopKillstreak);
		}
	}
	else if (!strcmp(eventName, "teamplay_round_start"))
	{
		m_BluTopKillstreak = 0;
		m_BluTopKillstreakPlayer = 0;
		m_RedTopKillstreak = 0;
		m_RedTopKillstreakPlayer = 0;
	}

	return true;
}