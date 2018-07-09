#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>

#include <memory>

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
};