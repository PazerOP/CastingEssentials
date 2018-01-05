#pragma once

#include "PluginBase/Modules.h"

class ConVar;
class IConVar;

class SteamTools : public Module<SteamTools>
{
public:
	SteamTools();

	static bool CheckDependencies();

private:
	ConVar *rich_presence_status;
	void ChangeRichPresenceStatus(IConVar *var, const char *pOldValue, float flOldValue);
};