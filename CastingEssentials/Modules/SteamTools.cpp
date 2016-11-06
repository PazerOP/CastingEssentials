#include "SteamTools.h"

#include <convar.h>
#include <steam/steam_api.h>

#include "PluginBase/Interfaces.h"

SteamTools::SteamTools()
{
	rich_presence_status = new ConVar("ce_steamtools_rich_presence", "", FCVAR_NONE, "the rich presence status displayed to Steam", [](IConVar *var, const char *pOldValue, float flOldValue) { GetModule()->ChangeRichPresenceStatus(var, pOldValue, flOldValue); });
}

bool SteamTools::CheckDependencies()
{
	bool ready = true;

	if (!Interfaces::AreSteamLibrariesAvailable())
	{
		PluginWarning("Required Steam libraries for module %s not available!\n", GetModuleName());
		ready = false;
	}

	return ready;
}

void SteamTools::ChangeRichPresenceStatus(IConVar *var, const char *pOldValue, float flOldValue)
{
	Interfaces::GetSteamAPIContext()->SteamFriends()->ClearRichPresence();
	Interfaces::GetSteamAPIContext()->SteamFriends()->SetRichPresence("status", rich_presence_status->GetString());
}