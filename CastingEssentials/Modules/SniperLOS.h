#pragma once

#include "PluginBase/EntityOffset.h"
#include "PluginBase/Modules.h"

#include <convar.h>
#include <igamesystem.h>

#include <optional>

class SniperLOS : public Module<SniperLOS>
{
public:
	SniperLOS();

	static bool CheckDependencies();

private:
	static EntityTypeChecker s_SniperRifleType;

	class LOSRenderer : public CAutoGameSystemPerFrame
	{
	public:
		const char* Name() override { return "SniperLOSRenderer"; }

		void PostRender() override;
	};
	std::optional<LOSRenderer> m_LOSRenderer;

	void ToggleEnabled(const ConVar* var);

	ConVar ce_sniperlos_enabled;
	ConVar ce_sniperlos_width;
	ConVar ce_sniperlos_color_blu;
	ConVar ce_sniperlos_color_red;

	Color m_BeamColorBlue;
	Color m_BeamColorRed;
};
