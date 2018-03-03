#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>
#include <igameevents.h>
#include <shareddefs.h>

#include <vector>

class C_BaseCombatCharacter; 
class CDamageAccountPanel;
class C_TFPlayer;

class HitEvents final : public Module<HitEvents>, IGameEventListener2
{
public:
	HitEvents();

	void OnTick(bool bInGame) override;

	static bool CheckDependencies() { return true; }

protected:
	void FireGameEvent(IGameEvent* event) override;

private:
	std::vector<IGameEvent*> m_EventsToIgnore;

	void DisplayDamageFeedbackOverride(CDamageAccountPanel* pThis, C_TFPlayer* pAttacker, C_BaseCombatCharacter* pVictim, int iDamageAmount, int iHealth, bool unknown);

	ConVar ce_hitevents_enabled;
	ConVar ce_hitevents_debug;

	IGameEvent* TriggerPlayerHurt(int playerEntIndex, int damage);
};