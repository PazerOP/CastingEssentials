#pragma once
#include "PluginBase/Modules.h"

#include <convar.h>
#include <shareddefs.h>

class IClientNetworkable;

namespace vgui
{
	class Panel;
	class ImagePanel;
	class EditablePanel;
	typedef unsigned int VPANEL;
}

class LoadoutIcons : public Module<LoadoutIcons>
{
public:
	LoadoutIcons();

	static bool CheckDependencies();
	static constexpr __forceinline const char* GetModuleName() { return "Loadout Icons"; }

protected:
	void OnTick(bool ingame) override;

private:
	void GatherWeapons();
	void DrawIcons();
	void PlayerPanelUpdateIcons(vgui::EditablePanel* playerPanel);

	static int GetPlayerIndex(vgui::EditablePanel* playerPanel);

	ConVar ce_loadout_enabled;
	ConVar ce_loadout_filter_active_red;
	ConVar ce_loadout_filter_active_blu;
	ConVar ce_loadout_filter_inactive_red;
	ConVar ce_loadout_filter_inactive_blu;

	enum ItemIndex
	{
		IDX_1,
		IDX_2,
		IDX_3,
		IDX_4,
		IDX_5,

		IDX_ACTIVE,

		ITEM_COUNT,
	};

	static constexpr const char* LOADOUT_ICONS[ITEM_COUNT][2] =
	{
		{ "LoadoutIconsItem1Red", "LoadoutIconsItem1Blue" },
		{ "LoadoutIconsItem2Red", "LoadoutIconsItem2Blue" },
		{ "LoadoutIconsItem3Red", "LoadoutIconsItem3Blue" },
		{ "LoadoutIconsItem4Red", "LoadoutIconsItem4Blue" },
		{ "LoadoutIconsItem5Red", "LoadoutIconsItem5Blue" },
		{ "LoadoutIconsActiveItemRed", "LoadoutIconsActiveItemBlue" },
	};

	byte m_ActiveWeaponIndices[MAX_PLAYERS];	// Index into m_Weapons
	int m_Weapons[MAX_PLAYERS][ITEM_COUNT];
};