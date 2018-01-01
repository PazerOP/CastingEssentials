#pragma once
#include "PluginBase/Modules.h"

#include <shareddefs.h>

class ConVar;
class ConCommand;
class IClientNetworkable;

namespace vgui
{
	class Panel;
	class ImagePanel;
	class EditablePanel;
	typedef unsigned int VPANEL;
}

class LoadoutIcons : public Module
{
public:
	LoadoutIcons();

	static bool CheckDependencies();
	static LoadoutIcons* GetModule() { return Modules().GetModule<LoadoutIcons>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<LoadoutIcons>().c_str(); }

protected:
	void OnTick(bool ingame) override;

private:
	void GatherWeapons();
	void DrawIcons();
	void PlayerPanelUpdateIcons(vgui::EditablePanel* playerPanel);

	static int GetPlayerIndex(vgui::EditablePanel* playerPanel);

	static int GetWeaponDefinitionIndex(IClientNetworkable* networkable);

	ConVar* ce_loadout_enabled;
	ConVar* ce_loadout_filter_active_red;
	ConVar* ce_loadout_filter_active_blu;
	ConVar* ce_loadout_filter_inactive_red;
	ConVar* ce_loadout_filter_inactive_blu;

	enum ItemIndex
	{
		IDX_1,
		IDX_2,
		IDX_3,
		IDX_4,
		IDX_5,

		IDX_ACTIVE
	};

	static constexpr const char* LOADOUT_ICONS[] = {
		"LoadoutIconsItem1",
		"LoadoutIconsItem2",
		"LoadoutIconsItem3",
		"LoadoutIconsItem4",
		"LoadoutIconsItem5",
		"LoadoutIconsActiveItem",
	};

	byte m_ActiveWeaponIndices[MAX_PLAYERS];	// Index into m_Weapons
	int m_Weapons[MAX_PLAYERS][arraysize(LOADOUT_ICONS)];
};