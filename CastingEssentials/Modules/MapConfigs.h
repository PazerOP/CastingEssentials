#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>

class MapConfigs final : public Module<MapConfigs>
{
public:
	MapConfigs();
	static constexpr __forceinline const char* GetModuleName() { return "Map Configs"; }

private:
	ConVar ce_mapconfigs_enabled;

	void LevelInit() override;
};