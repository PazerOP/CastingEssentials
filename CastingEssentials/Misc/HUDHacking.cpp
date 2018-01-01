#include "HUDHacking.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"

#include <client/iclientmode.h>
#include <KeyValues.h>
#include <vgui_controls/Panel.h>

vgui::VPANEL HUDHacking::GetSpecGUI()
{
	auto clientMode = Interfaces::GetClientMode();
	if (!clientMode)
		return 0;

	auto viewport = clientMode->GetViewport();
	if (!viewport)
		return 0;

	auto viewportPanel = viewport->GetVPanel();
	const auto viewportChildCount = g_pVGuiPanel->GetChildCount(viewportPanel);
	for (int specguiIndex = 0; specguiIndex < viewportChildCount; specguiIndex++)
	{
		vgui::VPANEL specguiPanel = g_pVGuiPanel->GetChild(viewportPanel, specguiIndex);
		if (!strcmp(g_pVGuiPanel->GetName(specguiPanel), "specgui"))
			return specguiPanel;
	}

	return 0;
}

Player* HUDHacking::GetPlayerFromPanel(vgui::EditablePanel* playerPanel)
{
	if (!playerPanel)
		return nullptr;

	auto dv = HookManager::GetRawFunc_EditablePanel_GetDialogVariables()(playerPanel);

	const char* playername = dv->GetString("playername");
	Assert(playername);	// Not a spectator gui playerpanel?
	if (!playername)
		return nullptr;

	Player* foundPlayer = Player::GetPlayerFromName(playername);
	return foundPlayer;
}

Color* HUDHacking::GetFillColor(vgui::ImagePanel* imgPanel)
{
	return imgPanel ? (Color*)(((DWORD*)imgPanel) + 94) : nullptr;
}

Color* HUDHacking::GetDrawColor(vgui::ImagePanel* imgPanel)
{
	return imgPanel ? (Color*)(((DWORD*)imgPanel) + 95) : nullptr;
}
