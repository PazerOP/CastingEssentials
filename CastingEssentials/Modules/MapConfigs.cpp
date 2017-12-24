#include "MapConfigs.h"
#include "PluginBase/Interfaces.h"

#include <convar.h>
#include <cdll_int.h>

MapConfigs::MapConfigs()
{
	ce_mapconfigs_enabled = new ConVar("ce_mapconfigs_enabled", "0", FCVAR_NONE, "If 1, execs cfg/<mapname>.cfg on mapchange.");
}

void MapConfigs::LevelInitPreEntity()
{
	if (!ce_mapconfigs_enabled->GetBool())
		return;

	const char* const bspName = Interfaces::GetEngineClient()->GetLevelName();
	std::string mapName(bspName);
	mapName.erase(mapName.size() - 4, 4);	// remove .bsp
	mapName.erase(0, 5);					// remove /maps/

	if (Interfaces::GetEngineClient()->IsPlayingDemo() || Interfaces::GetEngineClient()->IsHLTV())
		Interfaces::GetEngineClient()->ExecuteClientCmd(strprintf("exec %s", mapName.c_str()).c_str());
}
