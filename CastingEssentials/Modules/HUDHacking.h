#pragma once
#include "PluginBase/EntityOffset.h"
#include "PluginBase/Modules.h"

#include <convar.h>

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
	ConVar ce_hud_forward_playerpanel_border;
	ConVar ce_hud_player_health_progressbars;
	ConVar ce_hud_player_status_effects;
	ConVar ce_hud_chargebars_enabled;

	ConVar ce_hud_chargebars_buff_banner_text;
	ConVar ce_hud_chargebars_battalions_backup_text;
	ConVar ce_hud_chargebars_concheror_text;

	enum class BannerType
	{
		BuffBanner = 129,
		BattalionsBackup = 226,
		Concheror = 354,
	};

	// Static, because this is only used in GetPlayerFromPanel and we don't want to require
	// that HUDHacking module was loaded successfully just to use this completely independent function.
	static ConVar ce_hud_debug_unassociated_playerpanels;

	void OnTick(bool inGame) override;
	void UpdatePlayerPanels();
	static void ForwardPlayerPanelBorder(vgui::VPANEL playerVPanel, vgui::EditablePanel* playerPanel);
	static void UpdatePlayerHealth(vgui::VPANEL playerVPanel, vgui::EditablePanel* playerPanel, const Player& player);
	static void UpdateStatusEffect(vgui::VPANEL playerVPanel, vgui::EditablePanel* playerPanel, const Player& player);
	void UpdateBanner(vgui::VPANEL playerVPanel, vgui::EditablePanel* playerPanel, const Player& player);

	static bool GetBannerInfo(const Player& player, BannerType& type, float& charge);

	int m_ProgressBarApplySettingsHook;
	static void ProgressBarApplySettingsHook(vgui::ProgressBar* pThis, KeyValues* pSettings);

	static EntityOffset<float> s_RageMeter;
};