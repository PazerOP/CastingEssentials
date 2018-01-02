#include "HUDHacking.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"

#include <client/iclientmode.h>
#include <KeyValues.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/ProgressBar.h>

HUDHacking::HUDHacking()
{
	m_ProgressBarApplySettingsHook = GetHooks()->AddHook<vgui_ProgressBar_ApplySettings>(std::bind(ProgressBarApplySettingsHook, std::placeholders::_1, std::placeholders::_2));
}

HUDHacking::~HUDHacking()
{
	if (m_ProgressBarApplySettingsHook && GetHooks()->RemoveHook<vgui_ProgressBar_ApplySettings>(m_ProgressBarApplySettingsHook, __FUNCTION__))
		m_ProgressBarApplySettingsHook = 0;

	Assert(!m_ProgressBarApplySettingsHook);
}

bool HUDHacking::CheckDependencies()
{
	return true;
}

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

int* HUDHacking::GetProgressDirection(vgui::ProgressBar* progressBar)
{
	return progressBar ? (int*)(((DWORD*)progressBar) + 88) : nullptr;
}

void HUDHacking::ProgressBarApplySettingsHook(vgui::ProgressBar* pThis, KeyValues* pSettings)
{
	const char* dirStr = pSettings->GetString("direction", "east");
	int& dirVar = *GetProgressDirection(pThis);

	if (!stricmp(dirStr, "north"))
		dirVar = vgui::ProgressBar::PROGRESS_NORTH;
	else if (!stricmp(dirStr, "south"))
		dirVar = vgui::ProgressBar::PROGRESS_SOUTH;
	else if (!stricmp(dirStr, "west"))
		dirVar = vgui::ProgressBar::PROGRESS_WEST;
	else	// east is default
		dirVar = vgui::ProgressBar::PROGRESS_EAST;

	// Always execute the real function after we run this hook
	GetHooks()->SetState<vgui_ProgressBar_ApplySettings>(Hooking::HookAction::IGNORE);
}
