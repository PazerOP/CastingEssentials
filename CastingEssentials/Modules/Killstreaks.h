#pragma once

#include "PluginBase/Entities.h"
#include "PluginBase/Modules.h"

#include <convar.h>
#include <shared/ehandle.h>

#include <array>
#include <memory>

class C_BaseEntity;

class Killstreaks : public Module<Killstreaks>
{
public:
	Killstreaks();

	static bool CheckDependencies();

protected:
	void OnTick(bool inGame) override;

private:
	class Panel;
	std::unique_ptr<Panel> panel;

	ConVar ce_killstreaks_enabled;
	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);

	ConVar ce_killstreaks_hide_firstperson_effects;

	static std::array<EntityOffset<int>, 4> s_PlayerStreaks;
	static EntityOffset<bool> s_MedigunHealing;
	static EntityOffset<CHandle<C_BaseEntity>> s_MedigunHealingTarget;
};