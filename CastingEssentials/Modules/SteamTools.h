#pragma once

#include "PluginBase/Modules.h"

#include "steam/steamclientpublic.h"

class ConVar;
class IConVar;

class SteamTools : public Module
{
public:
	SteamTools();

	static bool CheckDependencies();

	static SteamTools* GetModule() { return Modules().GetModule<SteamTools>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<SteamTools>().c_str(); }
private:
	ConVar *rich_presence_status;
	void ChangeRichPresenceStatus(IConVar *var, const char *pOldValue, float flOldValue);
};