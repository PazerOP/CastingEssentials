#pragma once

#include "PluginBase/Modules.h"

#include <shareddefs.h>

class HitEvents final : public Module<HitEvents>
{
public:
	HitEvents();

	void OnTick(bool bInGame) override;
	void LevelInitPreEntity() override;

	static bool CheckDependencies() { return true; }

private:
	std::unique_ptr<ConVar> ce_hitevents_enable;

	void TriggerPlayerHurt(int playerEntIndex, int damage);

	int m_LastDamage[MAX_PLAYERS];
};