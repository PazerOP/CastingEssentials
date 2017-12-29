#pragma once
#include "PluginBase/Modules.h"

#include <shareddefs.h>

class ConVar;
class ConCommand;
class IClientNetworkable;

namespace vgui
{
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
	void ReloadSettings();
	void DeleteIconPanels();

	void GatherWeapons();
	void DrawIcons();
	bool PlayerPanelHasIcons(vgui::EditablePanel* player);
	void PlayerPanelInitIcons(vgui::EditablePanel* player);
	void PlayerPanelUpdateIcons(vgui::EditablePanel* player);

	static int GetPlayerIndex(vgui::EditablePanel* playerPanel);

	static int GetWeaponDefinitionIndex(IClientNetworkable* networkable);

	static vgui::VPANEL GetSpecGUI();

	ConVar* ce_loadout_enabled;
	ConCommand* ce_loadout_reload_settings;

	int m_ActiveWeapons[MAX_PLAYERS];
	int m_Weapons[MAX_PLAYERS][3];

	static constexpr int IMG_PANEL_COUNT = 3;
	static constexpr const char* LOADOUT_IMG_NAMES[] = {
		"LoadoutIconsItem1",
		"LoadoutIconsItem2",
		"LoadoutIconsItem3",
	};

	static_assert(arraysize(LOADOUT_IMG_NAMES) == IMG_PANEL_COUNT, "You forgot to update the array");
};