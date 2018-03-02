#include "HitEvents.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFPlayerResource.h"

#include <client/c_baseentity.h>
#include <client/c_baseplayer.h>
#include <client/cdll_client_int.h>
#include <igameevents.h>
#include <toolframework/ienginetool.h>

HitEvents::HitEvents() :
	ce_hitevents_enabled("ce_hitevents_enabled", "0"),
	ce_hitevents_debug("ce_hitevents_debug", "0")
{
}

IGameEvent* HitEvents::TriggerPlayerHurt(int playerEntIndex, int damage)
{
	IGameEvent* hurtEvent = gameeventmanager->CreateEvent("player_hurt");
	if (hurtEvent)
	{
		Player* player = Player::GetPlayer(playerEntIndex, __FUNCTION__);
		Player* localPlayer = Player::GetLocalPlayer();
		hurtEvent->SetInt("userid", player->GetUserID());
		hurtEvent->SetInt("attacker", localPlayer->GetUserID());
		//hurtEvent->SetInt("attacker", player->GetUserID());
		//hurtEvent->SetInt("userid", localPlayer->GetUserID());
		hurtEvent->SetInt("health", player->GetHealth());
		hurtEvent->SetInt("damageamount", damage);

		hurtEvent->SetInt("custom", 0);
		hurtEvent->SetBool("showdisguisedcrit", false);
		hurtEvent->SetBool("crit", false);
		hurtEvent->SetBool("minicrit", false);
		hurtEvent->SetBool("allseecrit", false);
		hurtEvent->SetInt("weaponid", 10);
		hurtEvent->SetInt("bonuseffect", 0);
	}

	return hurtEvent;
}

void HitEvents::FireGameEvent(IGameEvent* event)
{
	if (stricmp(event->GetName(), "player_hurt"))
		return;

	// Don't die to infinite recursion
	if (auto found = std::find(m_EventsToIgnore.begin(), m_EventsToIgnore.end(), event); found != m_EventsToIgnore.end())
	{
		m_EventsToIgnore.erase(found);
		return;
	}

	if (event->GetInt("userid") == event->GetInt("attacker"))
		return;	// Early-out self damage

	Player* localPlayer = Player::GetLocalPlayer();
	if (!localPlayer)
		return;

	if (auto actualLocalPlayer = Interfaces::GetLocalPlayer())
	{
		auto entindex1 = localPlayer->entindex();
		auto entindex2 = actualLocalPlayer->entindex();
		Assert(entindex1 == entindex2);
	}

	auto specTarget = Player::AsPlayer(localPlayer->GetObserverTarget());
	if (!specTarget || specTarget->GetUserID() != event->GetInt("attacker"))
		return;

	/*{
		auto newEvent = TriggerPlayerHurt(specTarget->entindex(), 123);
		m_EventsToIgnore.push_back(newEvent);
		gameeventmanager->FireEventClientSide(newEvent);
		return;
	}*/

	if (ce_hitevents_debug.GetBool())
	{
		std::stringstream msg;
		msg << std::boolalpha;

		auto attackedPlayer = Player::GetPlayerFromUserID(event->GetInt("userid"));
		auto attackingPlayer = Player::GetPlayerFromUserID(event->GetInt("attacker"));

		msg << "Received " << event->GetName() << " game event:\n"
			<< "\tuserid:            " << event->GetInt("userid") << " (" << (attackedPlayer ? attackedPlayer->GetName() : "<???>") << ")\n"
			<< "\tattacker:          " << event->GetInt("attacker") << " (" << (attackingPlayer ? attackingPlayer->GetName() : "<???>") << ")\n"
			<< "\tdamageamount:      " << event->GetInt("damageamount") << '\n';

		if (ce_hitevents_debug.GetInt() > 1)
		{
			msg << "\thealth:            " << event->GetInt("health") << '\n'
				<< "\tcustom:            " << event->GetInt("custom") << '\n'
				<< "\tshowdisguisedcrit: " << event->GetBool("showdisguisedcrit") << '\n'
				<< "\tcrit:              " << event->GetBool("crit") << '\n'
				<< "\tminicrit:          " << event->GetBool("minicrit") << '\n'
				<< "\tallseecrit:        " << event->GetBool("allseecrit") << '\n'
				<< "\tweaponid:          " << event->GetInt("weaponid") << '\n'
				<< "\tbonuseffect:       " << event->GetInt("bonuseffect") << '\n';
		}

		PluginMsg("%s", msg.str().c_str());
	}

	// Clone event
	IGameEvent* newEvent = gameeventmanager->CreateEvent("player_hurt");
	newEvent->SetInt("userid", event->GetInt("userid"));
	newEvent->SetInt("health", event->GetInt("health"));
	newEvent->SetInt("attacker", event->GetInt("attacker"));
	newEvent->SetInt("damageamount", event->GetInt("damageamount"));

	newEvent->SetInt("custom", event->GetInt("custom"));
	newEvent->SetBool("showdisguisedcrit", event->GetBool("showdisguisedcrit"));
	newEvent->SetBool("crit", event->GetBool("crit"));
	newEvent->SetBool("minicrit", event->GetBool("minicrit"));
	newEvent->SetBool("allseecrit", event->GetBool("allseecrit"));
	newEvent->SetInt("weaponid", event->GetInt("weaponid"));
	newEvent->SetInt("bonuseffect", event->GetInt("bonuseffect"));

	// Override attacker
	newEvent->SetInt("attacker", localPlayer->GetUserID());
	/*Player* localPlayer = Player::GetLocalPlayer();
	if (localPlayer)
	{
		auto attacker = Player::AsPlayer(localPlayer->GetObserverTarget());
		if (attacker && attacker->GetUserID() == newEvent->GetInt("attacker"))
			newEvent->SetInt("attacker", localPlayer->GetUserID());
	}*/

	if (ce_hitevents_debug.GetInt())
	{
		auto attackerUserID = newEvent->GetInt("attacker");
		PluginMsg("Rebroadcasting with attacker = %i (%s)\n", attackerUserID, Player::GetPlayerFromUserID(attackerUserID)->GetName());
	}

	m_EventsToIgnore.push_back(newEvent);

	//VariablePusher<C_BasePlayer*> localPlayerPusher(Interfaces::GetLocalPlayer(), );
	gameeventmanager->FireEventClientSide(newEvent);
}

void HitEvents::OnTick(bool bInGame)
{
	if (bInGame && ce_hitevents_enabled.GetBool() && !gameeventmanager->FindListener(this, "player_hurt"))
		gameeventmanager->AddListener(this, "player_hurt", false);
}