#include "HUDHacking.h"

#include "Modules/ItemSchema.h"

#include "PluginBase/Entities.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"

#include <client/c_basecombatweapon.h>
#include <client/iclientmode.h>
#include <KeyValues.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/ProgressBar.h>

#include <algorithm>

// Dumb macro names
#undef min
#undef max

EntityOffset<float> HUDHacking::s_RageMeter;

ConVar HUDHacking::ce_hud_debug_unassociated_playerpanels("ce_hud_debug_unassociated_playerpanels", "0", FCVAR_NONE, "Print debug messages to the console when a player cannot be found for a given playerpanel.");

HUDHacking::HUDHacking() :
	ce_hud_forward_playerpanel_border("ce_hud_forward_playerpanel_border", "0", FCVAR_NONE, "Sets the border of [playerpanel]->PanelColorBG to the same value as [playerpanel]."),
	ce_hud_player_health_progressbars("ce_hud_player_health_progressbars", "0", FCVAR_NONE, "Enables [playerpanel]->PlayerHealth[Overheal](Red/Blue) ProgressBars."),
	ce_hud_player_status_effects("ce_hud_player_status_effects", "0", FCVAR_NONE, "Update status effect ImagePanel: [playerpanel]->StatusEffectIcon(Red/Blue)"),
	ce_hud_chargebars_enabled("ce_hud_chargebars_enabled", "0", FCVAR_NONE, "Enable showing banner charge status (progress bar + label) in playerpanels."),

	ce_hud_chargebars_buff_banner_text("ce_hud_chargebars_buff_banner_text", "#TF_Unique_Achievement_SoldierBuff", FCVAR_NONE, "Text to use for the Buff Banner for the %banner% dialog variable on playerpanels."),
	ce_hud_chargebars_battalions_backup_text("ce_hud_chargebars_battalions_backup_text", "#TF_TheBattalionsBackup", FCVAR_NONE, "Text to use for the Battalion's Backup for the %banner% dialog variable on playerpanels."),
	ce_hud_chargebars_concheror_text("ce_hud_chargebars_concheror_text", "#TF_SoldierSashimono", FCVAR_NONE, "Text to use for the Concheror for the %banner% dialog variable on playerpanels."),

	m_ApplySettingsHook(std::bind(ProgressBarApplySettingsHook, std::placeholders::_1, std::placeholders::_2), true)
{
}

bool HUDHacking::CheckDependencies()
{
	{
		const auto playerClass = Entities::GetClientClass("CTFPlayer");
		s_RageMeter = Entities::GetEntityProp<float>(playerClass, "m_flRageMeter");
	}

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
	{
		PluginWarning("NULL playerPanel in " __FUNCTION__ "()\n");
		return nullptr;
	}

	auto dv = HookManager::GetRawFunc<HookFunc::vgui_EditablePanel_GetDialogVariables>()(playerPanel);

	const char* playername = dv->GetString("playername");
	Assert(playername);	// Not a spectator gui playerpanel?
	if (!playername)
		return nullptr;

	Player* foundPlayer = Player::GetPlayerFromName(playername);

	if (!foundPlayer && ce_hud_debug_unassociated_playerpanels.GetBool())
		PluginWarning("Unable to find player %s for panel %s\n", playername, playerPanel->GetName());

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
void HUDHacking::OnTick(bool inGame)
{
	if (!inGame)
		return;

	UpdatePlayerPanels();
}

void HUDHacking::UpdatePlayerPanels()
{
	const auto forwardBorder = ce_hud_forward_playerpanel_border.GetBool();
	const auto playerHealthProgressBars = ce_hud_player_health_progressbars.GetBool();
	const auto statusEffects = ce_hud_player_status_effects.GetBool();
	const auto bannerStatus = ce_hud_chargebars_enabled.GetBool();

	if (!forwardBorder && !playerHealthProgressBars && !statusEffects && !bannerStatus)
		return;

	auto specguivpanel = GetSpecGUI();
	if (!specguivpanel)
		return;

	const auto specguiChildCount = g_pVGuiPanel->GetChildCount(specguivpanel);
	for (int playerPanelIndex = 0; playerPanelIndex < specguiChildCount; playerPanelIndex++)
	{
		vgui::VPANEL playerVPanel = g_pVGuiPanel->GetChild(specguivpanel, playerPanelIndex);
		const char* playerPanelName = g_pVGuiPanel->GetName(playerVPanel);
		if (!g_pVGuiPanel->IsVisible(playerVPanel) || strncmp(playerPanelName, "playerpanel", 11))	// Names are like "playerpanel13"
			continue;

		vgui::EditablePanel* playerPanel = assert_cast<vgui::EditablePanel*>(g_pVGuiPanel->GetPanel(playerVPanel, "ClientDLL"));
		if (!playerPanel)
			continue;

		Assert(playerPanel->IsVisible() == g_pVGuiPanel->IsVisible(playerVPanel));

		if (forwardBorder)
			ForwardPlayerPanelBorder(playerVPanel, playerPanel);

		if (auto player = GetPlayerFromPanel(playerPanel))
		{
			if (playerHealthProgressBars)
				UpdatePlayerHealth(playerVPanel, playerPanel, *player);

			if (statusEffects)
				UpdateStatusEffect(playerVPanel, playerPanel, *player);

			UpdateBanner(bannerStatus, playerVPanel, playerPanel, *player);
		}
	}
}

void HUDHacking::UpdateStatusEffect(vgui::VPANEL playerVPanel, vgui::EditablePanel* playerPanel, const Player& player)
{
	bool isRedTeam;
	const char* team;
	switch (player.GetTeam())
	{
		case TFTeam::Red:
			team = "red";
			isRedTeam = true;
			break;
		case TFTeam::Blue:
			team = "blue";
			isRedTeam = false;
			break;

		default:
			return;
	}

	const char* iconBaseName = nullptr;
	if (player.CheckCondition(TFCond::TFCond_Ubercharged) || player.CheckCondition(TFCond::TFCond_UberchargeFading))
		iconBaseName = "../castingessentials/statuseffects/ubered_%s";
	else if (player.CheckCondition(TFCond::TFCond_Kritzkrieged))
		iconBaseName = "../castingessentials/statuseffects/kritzkrieged_%s";
	else if (player.CheckCondition(TFCond::TFCond_MegaHeal))
		iconBaseName = "../castingessentials/statuseffects/quickfixed_%s";
	else if (player.CheckCondition(TFCond::TFCond_UberBulletResist))
		iconBaseName = "../castingessentials/statuseffects/vaccinated_%s_bullet";
	else if (player.CheckCondition(TFCond::TFCond_UberBlastResist))
		iconBaseName = "../castingessentials/statuseffects/vaccinated_%s_explosive";
	else if (player.CheckCondition(TFCond::TFCond_UberFireResist))
		iconBaseName = "../castingessentials/statuseffects/vaccinated_%s_fire";
	else if (player.CheckCondition(TFCond::TFCond_Buffed))
		iconBaseName = "../castingessentials/statuseffects/buff_banner_%s";
	else if (player.CheckCondition(TFCond::TFCond_RegenBuffed))
		iconBaseName = "../castingessentials/statuseffects/concheror_%s";
	else if (player.CheckCondition(TFCond::TFCond_DefenseBuffed))
		iconBaseName = "../castingessentials/statuseffects/battalions_backup_%s";
	else if (player.CheckCondition(TFCond::TFCond_Bleeding))
		iconBaseName = "../castingessentials/statuseffects/bleeding_%s";
	else if (player.CheckCondition(TFCond::TFCond_MarkedForDeath) || player.CheckCondition(TFCond::TFCond_MarkedForDeathSilent))
		iconBaseName = "../castingessentials/statuseffects/marked_for_death_%s";

	auto icon = dynamic_cast<vgui::ImagePanel*>(FindChildByName(playerVPanel, isRedTeam ? "StatusEffectIconRed" : "StatusEffectIconBlue"));
	if (!icon)
		return;

	if (iconBaseName)
	{
		char buf[128];
		sprintf_s(buf, iconBaseName, team);

		icon->SetVisible(true);

		HookManager::GetRawFunc<HookFunc::vgui_ImagePanel_SetImage>()(icon, buf);
	}
	else
	{
		icon->SetVisible(false);
	}
}

void HUDHacking::UpdateBanner(bool enabled, vgui::VPANEL playerVPanel, vgui::EditablePanel* playerPanel, const Player& player)
{
	if (!enabled)
	{
		playerPanel->SetDialogVariable(WEAPON_CHARGE_AMOUNT, "");
		playerPanel->SetDialogVariable(WEAPON_CHARGE_NAME, "");
		return;
	}

	bool isRedTeam;
	bool shouldShowInfo = true;
	switch (player.GetTeam())
	{
		case TFTeam::Red:
			isRedTeam = true;
			break;
		case TFTeam::Blue:
			isRedTeam = false;
			break;

		default:
			shouldShowInfo = false;
			return;
	}

	BannerType type;
	float charge;
	if (shouldShowInfo)
		shouldShowInfo = player.IsAlive() && GetBannerInfo(player, type, charge);

	const char* bannerString = "";
	if (shouldShowInfo)
	{
		switch (type)
		{
			case BannerType::BattalionsBackup:    bannerString = ce_hud_chargebars_battalions_backup_text.GetString(); break;
			case BannerType::BuffBanner:          bannerString = ce_hud_chargebars_buff_banner_text.GetString(); break;
			case BannerType::Concheror:           bannerString = ce_hud_chargebars_concheror_text.GetString(); break;
		}
	}

	auto localized = g_pVGuiLocalize->FindAsUTF8(bannerString);
	playerPanel->SetDialogVariable(WEAPON_CHARGE_NAME, localized ? localized : bannerString);

	// Set charge level as a percentage
	{
		char buf[32];
		if (shouldShowInfo)
			sprintf_s(buf, "%i%%", (int)charge);
		else
			buf[0] = '\0';

		playerPanel->SetDialogVariable(WEAPON_CHARGE_AMOUNT, buf);
	}

	auto chargebar = dynamic_cast<vgui::ContinuousProgressBar*>(FindChildByName(playerVPanel, isRedTeam ? "WeaponChargeRed" : "WeaponChargeBlue"));
	if (chargebar)
		chargebar->SetVisible(shouldShowInfo);
}

bool HUDHacking::GetBannerInfo(const Player& player, BannerType& type, float& charge)
{
	if (player.GetClass() != TFClassType::Soldier)
		return false;

	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		C_BaseCombatWeapon* weapon = player.GetWeapon(i);
		if (!weapon)
			continue;

		// Make sure this is a banner of some type
		type = (BannerType)ItemSchema::GetModule()->GetBaseItemID(Entities::GetItemDefinitionIndex(weapon));
		switch (type)
		{
			case BannerType::BuffBanner:
			case BannerType::BattalionsBackup:
			case BannerType::Concheror:
				charge = s_RageMeter.GetValue(player.GetEntity());
				return true;
		}
	}

	return false;
}

void HUDHacking::ForwardPlayerPanelBorder(vgui::VPANEL playerVPanel, vgui::EditablePanel* playerPanel)
{
	auto border = playerPanel->GetBorder();
	if (!border)
		return;

	auto colorBGPanel = FindChildByName(playerVPanel, "PanelColorBG");
	if (!colorBGPanel)
		return;

	colorBGPanel->SetBorder(border);
}

void HUDHacking::UpdatePlayerHealth(vgui::VPANEL playerVPanel, vgui::EditablePanel* playerPanel, const Player& player)
{
	const auto health = player.IsAlive() ? player.GetHealth() : 0;
	const auto maxHealth = player.GetMaxHealth();
	const auto healthProgress = std::min<float>(1, health / (float)maxHealth);
	const auto overhealProgress = RemapValClamped(health, maxHealth, player.GetMaxOverheal(), 0, 1);

	struct ProgressBarName
	{
		constexpr ProgressBarName(const char* name, bool overheal, bool inverse) :
			m_Name(name), m_Overheal(overheal), m_Inverse(inverse)
		{
		}

		const char* m_Name;
		bool m_Overheal;
		bool m_Inverse;
	};

	static constexpr ProgressBarName s_ProgressBars[] =
	{
		ProgressBarName("PlayerHealthRed", false, false),
		ProgressBarName("PlayerHealthInverseRed", false, true),
		ProgressBarName("PlayerHealthBlue", false, false),
		ProgressBarName("PlayerHealthInverseBlue", false, true),

		ProgressBarName("PlayerHealthOverhealRed", true, false),
		ProgressBarName("PlayerHealthInverseOverhealRed", true, true),
		ProgressBarName("PlayerHealthOverhealBlue", true, false),
		ProgressBarName("PlayerHealthInverseOverhealBlue", true, true)
	};

	// Show/hide progress bars
	for (const auto& bar : s_ProgressBars)
	{
		auto progressBar = dynamic_cast<vgui::ProgressBar*>(FindChildByName(playerVPanel, bar.m_Name));
		if (!progressBar)
			continue;

		float progress = bar.m_Overheal ? overhealProgress : healthProgress;
		if (bar.m_Inverse)
			progress = 1 - progress;

		progressBar->SetVisible(health > 0);

		auto messageKV = new KeyValues("SetProgress");
		messageKV->SetFloat("progress", progress);
		progressBar->PostMessage(progressBar, messageKV);
	}
}

void HUDHacking::ProgressBarApplySettingsHook(vgui::ProgressBar* pThis, KeyValues* pSettings)
{
	const char* dirStr = pSettings->GetString("direction", "east");

	if (!stricmp(dirStr, "north"))
		pThis->SetProgressDirection(vgui::ProgressBar::PROGRESS_NORTH);
	else if (!stricmp(dirStr, "south"))
		pThis->SetProgressDirection(vgui::ProgressBar::PROGRESS_SOUTH);
	else if (!stricmp(dirStr, "west"))
		pThis->SetProgressDirection(vgui::ProgressBar::PROGRESS_WEST);
	else	// east is default
		pThis->SetProgressDirection(vgui::ProgressBar::PROGRESS_EAST);

	// Always execute the real function after we run this hook
	GetHooks()->SetState<HookFunc::vgui_ProgressBar_ApplySettings>(Hooking::HookAction::IGNORE);
}
