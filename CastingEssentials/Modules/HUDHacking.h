#pragma once
#include "PluginBase/EntityOffset.h"
#include "PluginBase/HookManager.h"
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

class HUDHacking final : public Module<HUDHacking>
{
public:
	HUDHacking();

	static bool CheckDependencies();

	static vgui::VPANEL GetSpecGUI();
	static Player* GetPlayerFromPanel(vgui::EditablePanel* playerPanel);

	// Like normal FindChildByName, but doesn't care about the fact we're in a different module.
	static vgui::Panel* FindChildByName(vgui::VPANEL rootPanel, const char* name, bool recursive = false);

private:
	ConVar ce_hud_forward_playerpanel_border;
	ConVar ce_hud_player_health_progressbars;
	ConVar ce_hud_player_status_effects;
	ConVar ce_hud_player_status_effects_debug;
	ConVar ce_hud_chargebars_enabled;
	ConVar ce_hud_progressbar_directions;
	ConVar ce_hud_find_parent_elements;

	ConVar ce_hud_chargebars_buff_banner_text;
	ConVar ce_hud_chargebars_battalions_backup_text;
	ConVar ce_hud_chargebars_concheror_text;

	enum class BannerType
	{
		BuffBanner = 129,
		BattalionsBackup = 226,
		Concheror = 354,
	};

	enum class StatusEffect
	{
		None = -1,

		Ubered,
		Kritzed,
		Quickfixed,
		VaccinatorBullet,
		VaccinatorBlast,
		VaccinatorFire,
		BuffBanner,
		Concheror,
		Battalions,
		Bleeding,
		MarkedForDeath,

		COUNT,
	};

	static const char* GetStatusEffectFormatString(StatusEffect effect);
	static StatusEffect GetStatusEffect(const Player& player);

	static constexpr const char WEAPON_CHARGE_AMOUNT[] = "weaponchargeamount";
	static constexpr const char WEAPON_CHARGE_NAME[] = "weaponchargename";

	// Static, because this is only used in GetPlayerFromPanel and we don't want to require
	// that HUDHacking module was loaded successfully just to use this completely independent function.
	static ConVar ce_hud_debug_unassociated_playerpanels;

	void OnTick(bool inGame) override;
	void UpdatePlayerPanels();
	static void ForwardPlayerPanelBorder(vgui::VPANEL playerVPanel, vgui::EditablePanel* playerPanel);
	static void UpdatePlayerHealth(vgui::VPANEL playerVPanel, vgui::EditablePanel* playerPanel, const Player& player);
	void UpdateStatusEffect(vgui::VPANEL playerVPanel, vgui::EditablePanel* playerPanel, const Player& player);
	void UpdateBanner(bool enabled, vgui::VPANEL playerVPanel, vgui::EditablePanel* playerPanel, const Player& player);

	static bool GetBannerInfo(const Player& player, BannerType& type, float& charge);

	Hook<HookFunc::vgui_ProgressBar_ApplySettings> m_ApplySettingsHook;
	static void ProgressBarApplySettingsHook(vgui::ProgressBar* pThis, KeyValues* pSettings);

	Hook<HookFunc::vgui_Panel_FindChildByName> m_FindChildByNameHook;
	vgui::Panel* FindChildByNameOverride(vgui::Panel* pThis, const char* name, bool recurseDown);

	static EntityOffset<float> s_RageMeter;
};