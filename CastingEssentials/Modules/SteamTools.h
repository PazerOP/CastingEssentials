#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>

class SteamTools : public Module<SteamTools>
{
public:
	SteamTools();

	static bool CheckDependencies();

private:
	ConVar ce_steamtools_rich_presence;
	void ChangeRichPresenceStatus(IConVar *var, const char *pOldValue, float flOldValue);
};