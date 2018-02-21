#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>

#include <memory>

class Killstreaks : public Module<Killstreaks>
{
public:
	Killstreaks();

	static bool CheckDependencies();

private:
	class Panel;
	std::unique_ptr<Panel> panel;

	ConVar ce_killstreaks_enabled;
	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);
};