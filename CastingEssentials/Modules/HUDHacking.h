#pragma once
#include "PluginBase/Modules.h"

class Player;

namespace vgui
{
	typedef unsigned int VPANEL;
	class Panel;
	class EditablePanel;
	class ImagePanel;
	class ProgressBar;
}

class HUDHacking : public Module<HUDHacking>
{
public:
	HUDHacking();
	~HUDHacking();

	static bool CheckDependencies();

	static vgui::VPANEL GetSpecGUI();
	static Player* GetPlayerFromPanel(vgui::EditablePanel* playerPanel);

	// Like normal FindChildByName, but doesn't care about the fact we're in a different module.
	static vgui::Panel* FindChildByName(vgui::VPANEL rootPanel, const char* name, bool recursive = false);

	static Color* GetFillColor(vgui::ImagePanel* imgPanel);
	static Color* GetDrawColor(vgui::ImagePanel* imgPanel);

	static int* GetProgressDirection(vgui::ProgressBar* progressBar);

private:
	std::unique_ptr<ConVar> ce_hud_forward_playerpanel_border;
	std::unique_ptr<ConVar> ce_hud_player_health_progressbars;

	void OnTick(bool inGame) override;
	void ForwardPlayerPanelBorders();
	void UpdatePlayerHealths();

	int m_ProgressBarApplySettingsHook;
	static void ProgressBarApplySettingsHook(vgui::ProgressBar* pThis, KeyValues* pSettings);
};