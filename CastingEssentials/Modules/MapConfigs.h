#pragma once

#include "PluginBase/Modules.h"

class MapConfigs final : public Module<MapConfigs>
{
public:
	MapConfigs();
	
private:
	ConVar* ce_mapconfigs_enabled;

	void LevelInitPreEntity() override;
};