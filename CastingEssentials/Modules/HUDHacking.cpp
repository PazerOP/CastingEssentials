#include "HUDHacking.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"

#include <client/iclientmode.h>
#include <KeyValues.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/ProgressBar.h>

#include <algorithm>

// Dumb macro names
#undef min
#undef max

HUDHacking::HUDHacking()
{
	m_ProgressBarApplySettingsHook = GetHooks()->AddHook<vgui_ProgressBar_ApplySettings>(std::bind(ProgressBarApplySettingsHook, std::placeholders::_1, std::placeholders::_2));

	ce_hud_forward_playerpanel_border.reset(new ConVar("ce_hud_forward_playerpanel_border", "1", FCVAR_NONE, "Sets the border of [playerpanel]->PanelColorBG to the same value as [playerpanel]."));
	ce_hud_player_health_progressbars.reset(new ConVar("ce_hud_player_health_progressbars", "1", FCVAR_NONE, "Enables [playerpanel]->PlayerHealth[Overheal](Red/Blue) ProgressBars."));
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

vgui::Panel* HUDHacking::FindChildByName(vgui::VPANEL rootPanel, const char* name, bool recursive)
{
	const auto childCount = g_pVGuiPanel->GetChildCount(rootPanel);
	for (int i = 0; i < childCount; i++)
	{
		auto child = g_pVGuiPanel->GetChild(rootPanel, i);
		auto childName = g_pVGuiPanel->GetName(child);
		if (!strcmp(childName, name))
			return g_pVGuiPanel->GetPanel(child, "ClientDLL");

		if (recursive)
			FindChildByName(child, name, recursive);
	}

	return nullptr;
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

void HUDHacking::OnTick(bool inGame)
{
	if (!inGame)
		return;

	if (ce_hud_forward_playerpanel_border->GetBool())
		ForwardPlayerPanelBorders();

	if (ce_hud_player_health_progressbars->GetBool())
		UpdatePlayerHealths();
}

void HUDHacking::ForwardPlayerPanelBorders()
{
	auto specguivpanel = GetSpecGUI();
	if (!specguivpanel)
		return;

	const auto specguiChildCount = g_pVGuiPanel->GetChildCount(specguivpanel);
	for (int playerPanelIndex = 0; playerPanelIndex < specguiChildCount; playerPanelIndex++)
	{
		vgui::VPANEL playerPanel = g_pVGuiPanel->GetChild(specguivpanel, playerPanelIndex);
		const char* playerPanelName = g_pVGuiPanel->GetName(playerPanel);
		if (strncmp(playerPanelName, "playerpanel", 11))	// Names are like "playerpanel13"
			continue;

		vgui::EditablePanel* player = assert_cast<vgui::EditablePanel*>(g_pVGuiPanel->GetPanel(playerPanel, "ClientDLL"));
		auto border = player->GetBorder();
		if (!border)
			continue;

		auto colorBGPanel = FindChildByName(player->GetVPanel(), "PanelColorBG");
		if (!colorBGPanel)
			continue;

		colorBGPanel->SetBorder(border);
	}
}

void HUDHacking::UpdatePlayerHealths()
{
	auto specguivpanel = GetSpecGUI();
	if (!specguivpanel)
		return;

	const auto specguiChildCount = g_pVGuiPanel->GetChildCount(specguivpanel);
	for (int playerPanelIndex = 0; playerPanelIndex < specguiChildCount; playerPanelIndex++)
	{
		vgui::VPANEL playerVPanel = g_pVGuiPanel->GetChild(specguivpanel, playerPanelIndex);
		const char* playerPanelName = g_pVGuiPanel->GetName(playerVPanel);
		if (strncmp(playerPanelName, "playerpanel", 11))	// Names are like "playerpanel13"
			continue;

		vgui::EditablePanel* playerPanel = assert_cast<vgui::EditablePanel*>(g_pVGuiPanel->GetPanel(playerVPanel, "ClientDLL"));

		auto player = GetPlayerFromPanel(playerPanel);
		if (!player)
		{
			PluginWarning("Unable to find player for panel {0}", playerPanelName);
			continue;
		}

		const auto health = player->IsAlive() ? player->GetHealth() : 0;
		const auto maxHealth = player->GetMaxHealth();
		const auto healthProgress = std::min<float>(1, health / (float)maxHealth);
		const auto overhealProgress = RemapValClamped(health, maxHealth, player->GetMaxOverheal(), 0, 1);

		static constexpr const char* s_ProgressBarNames[] =
		{
			"PlayerHealthRed",
			"PlayerHealthBlue",
			"PlayerHealthOverhealRed",
			"PlayerHealthOverhealBlue"
		};

		// Show/hide progress bars
		for (size_t i = 0; i < std::size(s_ProgressBarNames); i++)
		{
			auto progressBar = dynamic_cast<vgui::ProgressBar*>(FindChildByName(playerVPanel, s_ProgressBarNames[i]));
			if (!progressBar)
				continue;

			progressBar->SetVisible(health > 0);

			auto messageKV = new KeyValues("SetProgress");
			messageKV->SetFloat("progress", i >= 2 ? overhealProgress : healthProgress);
			progressBar->PostMessage(progressBar, messageKV);
		}
	}
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
