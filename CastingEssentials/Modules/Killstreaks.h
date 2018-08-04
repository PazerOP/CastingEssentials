#pragma once

#include "PluginBase/Entities.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Modules.h"

#include <convar.h>
#include <shared/ehandle.h>

#include <array>
#include <map>

class C_BaseEntity;

class Killstreaks : public Module<Killstreaks>
{
public:
	Killstreaks();

	static bool CheckDependencies();
	static constexpr __forceinline const char* GetModuleName() { return "Killstreaks"; }

protected:
	void OnTick(bool inGame) override;

private:
	ConVar ce_killstreaks_enabled;
	ConVar ce_killstreaks_debug;
	void UpdateKillstreaks(bool inGame);

	ConVar ce_killstreaks_hide_firstperson_effects;
	void HideFirstPersonEffects() const;

	bool FireEventClientSideOverride(IGameEvent *event);

	int m_BluTopKillstreak;
	int m_BluTopKillstreakPlayer;
	int m_RedTopKillstreak;
	int m_RedTopKillstreakPlayer;

	std::map<int, int> m_CurrentKillstreaks;
	Hook<HookFunc::IGameEventManager2_FireEventClientSide> m_FireEventClientSideHook;

	static std::array<EntityOffset<int>, 4> s_PlayerStreaks;
	static EntityOffset<bool> s_MedigunHealing;
	static EntityOffset<CHandle<C_BaseEntity>> s_MedigunHealingTarget;
	static EntityTypeChecker s_MedigunType;
};