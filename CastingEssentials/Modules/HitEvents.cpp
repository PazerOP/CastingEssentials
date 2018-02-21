#include "HitEvents.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFPlayerResource.h"

#include <client/cdll_client_int.h>
#include <igameevents.h>
#include <toolframework/ienginetool.h>

HitEvents::HitEvents() :
	ce_hitevents_enable("ce_hitevents_enable", "0", FCVAR_HIDDEN)
{
}

void HitEvents::LevelInitPreEntity()
{
	for (size_t i = 0; i < std::size(m_LastDamage); i++)
		m_LastDamage[i] = -1;
}

void HitEvents::TriggerPlayerHurt(int playerEntIndex, int damage)
{
	IGameEvent* hurtEvent = gameeventmanager->CreateEvent("player_hurt");
	if (hurtEvent)
	{
		Player* player = Player::GetPlayer(playerEntIndex, __FUNCTION__);
		Player* localPlayer = Player::GetLocalPlayer();
		hurtEvent->SetInt("userid", player->GetUserID());
		hurtEvent->SetInt("health", player->GetHealth());
		hurtEvent->SetInt("attacker", localPlayer->GetUserID());
		hurtEvent->SetInt("damageamount", damage);

		hurtEvent->SetInt("custom", 0);
		hurtEvent->SetBool("showdisguisedcrit", false);
		hurtEvent->SetBool("crit", false);
		hurtEvent->SetBool("minicrit", false);
		hurtEvent->SetBool("allseecrit", false);
		hurtEvent->SetInt("weaponid", 10);
		hurtEvent->SetInt("bonuseffect", 0);

		gameeventmanager->FireEventClientSide(hurtEvent);
	}
}

void HitEvents::OnTick(bool bInGame)
{
	if (!bInGame || !ce_hitevents_enable.GetBool())
		return;

	int newDamage[MAX_PLAYERS];

	// Gather new damage
	const auto& playerResource = TFPlayerResource::GetPlayerResource();
	const auto maxClients = Interfaces::GetEngineTool()->GetMaxClients();
	for (int i = 0; i < maxClients; i++)
		newDamage[i] = playerResource->GetDamage(i + 1);

	// Compare against old damage
	for (int i = 0; i < maxClients; i++)
	{
		if (m_LastDamage[i] >= 0 && newDamage[i] > m_LastDamage[i])
		{
			const auto delta = newDamage[i] - m_LastDamage[i];

			//engine->Con_NPrintf(i, "%i", delta);

			TriggerPlayerHurt(i + 1, delta);
		}

		m_LastDamage[i] = newDamage[i];
	}
}