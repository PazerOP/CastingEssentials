#include "HUDHacking.h"

#include "Modules/ItemSchema.h"

#include "PluginBase/Entities.h"
#include "PluginBase/EntityOffsetIterator.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"

#include <client/c_basecombatweapon.h>
#include <client/game_controls/baseviewport.h>
#include <client/iclientmode.h>
#include <KeyValues.h>
#include <toolframework/ienginetool.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/ProgressBar.h>
#include <vprof.h>

#include <algorithm>

// Dumb macro names
#undef min
#undef max

MODULE_REGISTER(HUDHacking);

EntityOffset<float> HUDHacking::s_RageMeter;
EntityOffset<float> HUDHacking::s_EnergyDrinkMeter;
EntityOffset<float> HUDHacking::s_HypeMeter;
EntityOffset<float> HUDHacking::s_ChargeMeter;
EntityOffset<float> HUDHacking::s_CloakMeter;
EntityOffset<float> HUDHacking::s_MedigunChargeMeter;
EntityOffset<TFResistType> HUDHacking::s_MedigunResistType;
EntityOffset<float> HUDHacking::s_ItemChargeMeters[11];
EntityOffset<float> HUDHacking::s_WeaponEnergyMeter;
EntityOffset<float> HUDHacking::s_WeaponEffectBarRegenTime;
EntityOffset<float> HUDHacking::s_SpycicleMeltTime;
EntityOffset<float> HUDHacking::s_SpycicleRegenTime;
EntityTypeChecker HUDHacking::s_WearableShieldType;
EntityTypeChecker HUDHacking::s_RazorbackType;
EntityOffset<float> HUDHacking::s_CleanersCarbineCharge;

const HUDHacking::ChargeBarInfo HUDHacking::s_ChargeBarInfo[(int)ChargeBarType::COUNT] =
{
	ChargeBarInfo("soda_popper", 1, "the Soda Popper", "#TF_SodaPopper", 448, MeterType::Hype),
	ChargeBarInfo("baby_face", 4, "the Baby Face's Blaster", "#TF_Weapon_PEP_Scattergun", 772, MeterType::Hype),
	ChargeBarInfo("bonk", 2, "Bonk! Atomic Punch", "#TF_Unique_Achievement_EnergyDrink", 46, MeterType::EnergyDrink),
	ChargeBarInfo("crit_cola", 5, "the Crit-a-Cola", "#TF_Unique_EnergyDrink_CritCola", 163, MeterType::EnergyDrink),
	ChargeBarInfo("milk", 5, "Mad Milk", "#TF_MadMilk", 222, MeterType::EffectBar, 20),
	ChargeBarInfo("cleaver", 2, "the Flying Guillotine", "#TF_SD_Cleaver", 812, MeterType::EffectBar, 5),
	ChargeBarInfo("sandman", 3, "the Sandman", "#TF_Unique_Achievement_Bat", 44, MeterType::EffectBar, 10),
	ChargeBarInfo("wrap_assassin", 3, "the Wrap Assassin", "#TF_BallBuster", 648, MeterType::EffectBar, 7.5),

	ChargeBarInfo("banner_battalions", 1, "the Battalion's Backup", "#TF_TheBattalionsBackup", 226, MeterType::Rage),
	ChargeBarInfo("banner_buff", 1, "the Buff Banner", "#TF_Unique_Achievement_SoldierBuff", 129, MeterType::Rage),
	ChargeBarInfo("banner_conch", 1, "the Concheror", "#TF_SoldierSashimono", 354, MeterType::Rage),

	ChargeBarInfo("phlog", 2, "the Phlogistinator", "#TF_Phlogistinator", 594, MeterType::Rage),
	ChargeBarInfo("gas_passer", 1, "the Gas Passer", "#TF_GasPasser", 1180, MeterType::ItemCharge1),
	ChargeBarInfo("jetpack", 1, "the Thermal Thruster", "#TF_ThermalThruster", 1179, MeterType::ItemCharge1),

	ChargeBarInfo("shield_targe", 1, "the Chargin' Targe", "#TF_Unique_Achievement_Shield", 131, MeterType::Charge),
	ChargeBarInfo("shield_splendid", 1, "the Splendid Screen", "#TF_SplendidScreen", 406, MeterType::Charge),
	ChargeBarInfo("shield_tide", 1, "the Tide Turner", "#TF_TideTurner", 1099, MeterType::Charge),

	ChargeBarInfo("sandvich", 1, "the Sandvich", "#TF_Unique_Achievement_LunchBox", 42, MeterType::ItemCharge1),
	ChargeBarInfo("dalokohs", 1, "the Dalokohs Bar", "#TF_Unique_Lunchbox_Chocolate", 159, MeterType::ItemCharge1),
	ChargeBarInfo("steak_sandvich", 1, "the Buffalo Steak Sandvich", "#TF_BuffaloSteak", 311, MeterType::ItemCharge1),
	ChargeBarInfo("banana", 1, "the Second Banana", "#TF_Unique_Lunchbox_Banana", 1190, MeterType::ItemCharge1),

	ChargeBarInfo("medigun_uber", 1, "the Medi Gun", "#TF_Weapon_Medigun", 29, MeterType::MedigunCharge),
	ChargeBarInfo("medigun_kritz", 1, "the Kritzkrieg", "#TF_Unique_Achievement_Medigun1", 35, MeterType::MedigunCharge),
	ChargeBarInfo("medigun_quickfix", 1, "the Quick-Fix", "#TF_Unique_MediGun_QuickFix", 411, MeterType::MedigunCharge),
	ChargeBarInfo("medigun_vaccinator", 1, "the Vaccinator", "#TF_Unique_MediGun_Resist", 998, MeterType::MedigunCharge),

	ChargeBarInfo("hitmans_heatmaker", 2, "the Hitman's Heatmaker", "#TF_Pro_SniperRifle", 752, MeterType::Rage),
	ChargeBarInfo("jarate", 3, "Jarate", "#TF_Unique_Achievement_Jar", 58, MeterType::EffectBar, 20),
	ChargeBarInfo("razorback", 1, "the Razorback", "#TF_Unique_Backstab_Shield", 57, MeterType::ItemCharge1),
	ChargeBarInfo("cleaners_carbine", 1, "the Cleaner's Carbine", "#TF_Pro_SMG", 751, MeterType::CleanersCarbine),

	ChargeBarInfo("spycicle", 2, "the Spy-cicle", "#TF_SpyCicle", 649, MeterType::Spycicle),
	ChargeBarInfo("invis_watch", 1, "the Invis Watch", "#TF_Weapon_Watch", 30, MeterType::Cloak),
	ChargeBarInfo("cloak_and_dagger", 1, "the Cloak and Dagger", "#TF_Unique_Achievement_CloakWatch", 60, MeterType::Cloak),
	ChargeBarInfo("dead_ringer", 1, "the Dead Ringer", "#TF_Unique_Achievement_FeignWatch", 59, MeterType::Cloak),
};

static constexpr auto test = sizeof(HUDHacking);

ConVar HUDHacking::ce_hud_debug_unassociated_playerpanels("ce_hud_debug_unassociated_playerpanels", "0", FCVAR_NONE, "Print debug messages to the console when a player cannot be found for a given playerpanel.");

HUDHacking::HUDHacking() :
	ce_hud_forward_playerpanel_border("ce_hud_forward_playerpanel_border", "0", FCVAR_NONE, "Sets the border of [playerpanel]->PanelColorBG to the same value as [playerpanel]."),
	ce_hud_player_health_progressbars("ce_hud_player_health_progressbars", "0", FCVAR_NONE, "Enables [playerpanel]->PlayerHealth[Overheal](Red/Blue) ProgressBars."),
	ce_hud_player_status_effects("ce_hud_player_status_effects", "0", FCVAR_NONE, "Update status effect ImagePanel: [playerpanel]->StatusEffectIcon(Red/Blue)"),
	ce_hud_player_status_effects_debug("ce_hud_player_status_effects_debug", "0", FCVAR_NONE,
		"Shows a status effect icon for all players.", true, 0, true, (int)StatusEffect::COUNT),
	ce_hud_chargebars_enabled("ce_hud_chargebars_enabled", "0", FCVAR_NONE, "Enable showing banner charge status (progress bar + label) in playerpanels."),
	ce_hud_chargebars_debug("ce_hud_chargebars_debug", "0", FCVAR_NONE),

	ce_hud_progressbar_directions("ce_hud_progressbar_directions", "0", FCVAR_NONE,
		"Enables setting 'direction <north/east/south/west>' for vgui ProgressBar elements.",
		[](IConVar* var, const char*, float) { GetModule()->m_ApplySettingsHook.SetEnabled(static_cast<ConVar*>(var)->GetBool()); }),

	ce_hud_find_parent_elements("ce_hud_find_parent_elements", "0", FCVAR_NONE,
		"Enables moving a panel's search-by-name scope upwards by prefixing the name with '../' (like referencing a parent's sibling in a hud animation)",
		[](IConVar* var, const char*, float) { GetModule()->m_FindChildByNameHook.SetEnabled(static_cast<ConVar*>(var)->GetBool()); }),

	ce_hud_class_change_animations("ce_hud_class_change_animations", "0", FCVAR_NONE,
		"Runs PlayerPanel_ClassChangedRed/Blue hudanims on the playerpanels whenever a player changes class."),

	m_ApplySettingsHook(std::bind(ProgressBarApplySettingsHook, std::placeholders::_1, std::placeholders::_2)),
	m_FindChildByNameHook(std::bind(&HUDHacking::FindChildByNameOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
{
	for (int i = 0; i < (int)ChargeBarType::COUNT; i++)
		m_ChargeBarCvars[i].emplace(s_ChargeBarInfo[i]);
}

bool HUDHacking::CheckDependencies()
{
	{
		// CTFPlayer
		{
			const auto playerClass = Entities::GetClientClass("CTFPlayer");
			s_RageMeter = Entities::GetEntityProp<float>(playerClass, "m_flRageMeter");
			s_EnergyDrinkMeter = Entities::GetEntityProp<float>(playerClass, "m_flEnergyDrinkMeter");
			s_HypeMeter = Entities::GetEntityProp<float>(playerClass, "m_flHypeMeter");
			s_ChargeMeter = Entities::GetEntityProp<float>(playerClass, "m_flChargeMeter");
			s_CloakMeter = Entities::GetEntityProp<float>(playerClass, "m_flCloakMeter");

			char buf[32];
			for (int i = 0; i < 11; i++)
				s_ItemChargeMeters[i] = Entities::GetEntityProp<float>(playerClass, Entities::PropIndex(buf, "m_flItemChargeMeter", i));
		}

		// CTFWeaponBase
		{
			const auto weaponClass = Entities::GetClientClass("CTFWeaponBase");
			s_WeaponEnergyMeter = Entities::GetEntityProp<float>(weaponClass, "m_flEnergy");
			s_WeaponEffectBarRegenTime = Entities::GetEntityProp<float>(weaponClass, "m_flEffectBarRegenTime");
		}

		// CTFKnife
		{
			const auto spycicleClass = Entities::GetClientClass("CTFKnife");
			s_SpycicleRegenTime = Entities::GetEntityProp<float>(spycicleClass, "m_flKnifeRegenerateDuration");
			s_SpycicleMeltTime = Entities::GetEntityProp<float>(spycicleClass, "m_flKnifeMeltTimestamp");
		}

		s_MedigunChargeMeter = Entities::GetEntityProp<float>("CWeaponMedigun", "m_flChargeLevel");
		s_MedigunResistType = Entities::GetEntityProp<TFResistType>("CWeaponMedigun", "m_nChargeResistType");
		s_CleanersCarbineCharge = Entities::GetEntityProp<float>("CTFChargedSMG", "m_flMinicritCharge");

		s_WearableShieldType = Entities::GetTypeChecker("CTFWearableDemoShield");
		s_RazorbackType = Entities::GetTypeChecker("CTFWearableRazorback");
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

vgui::AnimationController* HUDHacking::GetAnimationController()
{
	auto clientMode = Interfaces::GetClientMode();
	if (!clientMode)
		return nullptr;

	auto viewportPanel = clientMode->GetViewport();
	if (!viewportPanel)
		return nullptr;

	auto viewport = dynamic_cast<CBaseViewport*>(viewportPanel);
	if (!viewport)
		return nullptr;

	return viewport->GetAnimationController();
}

Player* HUDHacking::GetPlayerFromPanel(vgui::EditablePanel* playerPanel)
{
	if (!playerPanel)
	{
		PluginWarning("NULL playerPanel in " __FUNCTION__ "()\n");
		return nullptr;
	}

	auto dv = playerPanel->GetDialogVariables();

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
float HUDHacking::GetChargeMeter(const ChargeBarInfo& info, Player& player, const IClientNetworkable& playerNetworkable,
	const IClientNetworkable& weapon) const
{
	if (ce_hud_chargebars_debug.GetBool())
	{
		int i = 0;

		{
			static const RecvTable* excludeTables[] =
			{
				Entities::FindRecvTable("DT_BaseFlex"),
				Entities::FindRecvTable("DT_AttributeList"),
				Entities::FindRecvTable("DT_EconEntity"),
			};

			for (const auto& prop : EntityOffsetIterable<float>(&weapon, std::begin(excludeTables), std::end(excludeTables)))
				engine->Con_NPrintf(i++, "%s: %1.2f", prop.first.c_str(), prop.second.GetValue(&weapon));

			i++;
		}

		engine->Con_NPrintf(i++, "rage: %1.2f", s_RageMeter.TryGetValue(&playerNetworkable, NAN));
		engine->Con_NPrintf(i++, "energy drink: %1.2f", s_EnergyDrinkMeter.TryGetValue(&playerNetworkable, NAN));
		engine->Con_NPrintf(i++, "hype: %1.2f", s_HypeMeter.TryGetValue(&playerNetworkable, NAN));
		engine->Con_NPrintf(i++, "charge: %1.2f", s_ChargeMeter.TryGetValue(&playerNetworkable, NAN));
		engine->Con_NPrintf(i++, "cloak: %1.2f", s_CloakMeter.TryGetValue(&playerNetworkable, NAN));

		engine->Con_NPrintf(i++, "weapon energy: %1.2f", s_WeaponEnergyMeter.TryGetValue(&weapon, NAN));

		engine->Con_NPrintf(i++, "effect bar regen time: %1.2f", s_WeaponEffectBarRegenTime.TryGetValue(&weapon, NAN));
		engine->Con_NPrintf(i++, "cleaners carbine charge: %1.2f", s_CleanersCarbineCharge.TryGetValue(&weapon, NAN));

		i++;

		for (int a = 0; a < 11; a++)
			engine->Con_NPrintf(i++, "item charge %i: %1.2f", a, s_ItemChargeMeters[a].TryGetValue(&playerNetworkable, NAN));
	}

	switch (info.m_Meter)
	{
		case MeterType::Rage:               return s_RageMeter.GetValue(&playerNetworkable);
		case MeterType::EnergyDrink:        return s_EnergyDrinkMeter.GetValue(&playerNetworkable);
		case MeterType::Hype:               return s_HypeMeter.GetValue(&playerNetworkable);
		case MeterType::Charge:             return s_ChargeMeter.GetValue(&playerNetworkable);
		case MeterType::MedigunCharge:      return s_MedigunChargeMeter.GetValue(&weapon) * 100;
		case MeterType::Cloak:              return s_CloakMeter.GetValue(&playerNetworkable);

		case MeterType::ItemCharge0:        return s_ItemChargeMeters[0].GetValue(&playerNetworkable);
		case MeterType::ItemCharge1:        return s_ItemChargeMeters[1].GetValue(&playerNetworkable);
		case MeterType::CleanersCarbine:    return player.GetState<PlayerState>().GetCleanersCarbineCharge();

		case MeterType::Spycicle:
		{
			auto regenTime = s_SpycicleRegenTime.GetValue(&weapon);
			auto meltTime = s_SpycicleMeltTime.GetValue(&weapon);
			auto clientTime = Interfaces::GetEngineTool()->ClientTime();

			if (clientTime >= (meltTime + regenTime))
				return 100;

			return ((clientTime - meltTime) / regenTime) * 100;
		}

		case MeterType::EffectBar:
		{
			auto regenTime = s_WeaponEffectBarRegenTime.GetValue(&weapon);
			Assert(info.m_EffectBarChargeTime > 0);
			auto clientTime = Interfaces::GetEngineTool()->ClientTime();

			if (clientTime >= regenTime)
				return 100;

			return 100 - ((regenTime - clientTime) / info.m_EffectBarChargeTime * 100);
		}
	}

	return (2.0f / 3) * 100;
}
const char* HUDHacking::GetStatusEffectFormatString(StatusEffect effect)
{
	switch (effect)
	{
		case StatusEffect::None:             return nullptr;
		case StatusEffect::Ubered:           return "%subered_%s";
		case StatusEffect::Kritzed:          return "%skritzkrieged_%s";
		case StatusEffect::Quickfixed:       return "%squickfixed_%s";
		case StatusEffect::VaccinatorBullet: return "%svaccinated_%s_bullet";
		case StatusEffect::VaccinatorBlast:  return "%svaccinated_%s_explosive";
		case StatusEffect::VaccinatorFire:   return "%svaccinated_%s_fire";
		case StatusEffect::BuffBanner:       return "%sbuff_banner_%s";
		case StatusEffect::Concheror:        return "%sconcheror_%s";
		case StatusEffect::Battalions:       return "%sbattalions_backup_%s";
		case StatusEffect::Bleeding:         return "%sbleeding_%s";
		case StatusEffect::MarkedForDeath:   return "%smarked_for_death_%s";
	}

	PluginWarning("Programmer error: Unknown StatusEffect %i in " __FUNCTION__ "()\n", (int)effect);
	return nullptr;
}

HUDHacking::StatusEffect HUDHacking::GetStatusEffect(const Player& player)
{
	if (player.IsAlive())
	{
		if (player.CheckCondition(TFCond::TFCond_Ubercharged) || player.CheckCondition(TFCond::TFCond_UberchargeFading))
			return StatusEffect::Ubered;
		else if (player.CheckCondition(TFCond::TFCond_Kritzkrieged))
			return StatusEffect::Kritzed;
		else if (player.CheckCondition(TFCond::TFCond_MegaHeal))
			return StatusEffect::Quickfixed;
		else if (player.CheckCondition(TFCond::TFCond_UberBulletResist))
			return StatusEffect::VaccinatorBullet;
		else if (player.CheckCondition(TFCond::TFCond_UberBlastResist))
			return StatusEffect::VaccinatorBlast;
		else if (player.CheckCondition(TFCond::TFCond_UberFireResist))
			return StatusEffect::VaccinatorFire;
		else if (player.CheckCondition(TFCond::TFCond_Buffed))
			return StatusEffect::BuffBanner;
		else if (player.CheckCondition(TFCond::TFCond_RegenBuffed))
			return StatusEffect::Concheror;
		else if (player.CheckCondition(TFCond::TFCond_DefenseBuffed))
			return StatusEffect::Battalions;
		else if (player.CheckCondition(TFCond::TFCond_Bleeding))
			return StatusEffect::Bleeding;
		else if (player.CheckCondition(TFCond::TFCond_MarkedForDeath) || player.CheckCondition(TFCond::TFCond_MarkedForDeathSilent))
			return StatusEffect::MarkedForDeath;
	}

	return StatusEffect::None;
}

void HUDHacking::OnTick(bool inGame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
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

			UpdateChargeBar(bannerStatus, playerVPanel, playerPanel, *player);

			if (ce_hud_class_change_animations.GetBool())
				UpdateClassChangeAnimations(playerVPanel, playerPanel, *player);
		}
	}
}

void HUDHacking::UpdateStatusEffect(vgui::VPANEL playerVPanel, vgui::EditablePanel* playerPanel, const Player& player)
{
	vgui::ImagePanel* icon;
	const char* team;
	bool isRedTeam;
	if (auto teamVal = player.GetTeam(); teamVal == TFTeam::Red)
	{
		team = "red";
		isRedTeam = true;
	}
	else if (teamVal == TFTeam::Blue)
	{
		team = "blue";
		isRedTeam = false;
	}
	else
		return;

	icon = dynamic_cast<vgui::ImagePanel*>(FindChildByName(playerVPanel, isRedTeam ? "StatusEffectIconRed" : "StatusEffectIconBlue"));
	if (!icon)
		return;

	StatusEffect effect;
	if (auto debug = ce_hud_player_status_effects_debug.GetInt())
		effect = (StatusEffect)(debug - 1);
	else
		effect = GetStatusEffect(player);

	if (effect != StatusEffect::None)
	{
		char buf[MAX_PATH];
		sprintf_s(buf, GetStatusEffectFormatString(effect), "../castingessentials/statuseffects/", team);

		icon->SetVisible(true);
		icon->SetImage(buf);
	}
	else
	{
		icon->SetVisible(false);
	}
}

#pragma warning(push)
#pragma warning(disable : 4701)	// Potentially uninitialized local variable used
void HUDHacking::UpdateChargeBar(bool enabled, vgui::VPANEL playerVPanel, vgui::EditablePanel* playerPanel, Player& player) const
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
			return;
			//shouldShowInfo = false;
			//break;
	}

	ChargeBarType type;
	float charge;
	if (shouldShowInfo)
		shouldShowInfo = player.IsAlive() && GetChargeBarData(player, type, charge);

	const char* bannerString = "";
	char iconPath[MAX_PATH];
	iconPath[0] = '\0';
	if (shouldShowInfo)
	{
		bannerString = m_ChargeBarCvars[(int)type]->m_TextCvar.GetString();
		sprintf_s(iconPath, "../castingessentials/chargebars/%s_%s", s_ChargeBarInfo[(int)type].m_Name, isRedTeam ? "red" : "blue");

		if (C_BaseCombatWeapon* medigun; type == ChargeBarType::MedigunVaccinator && (medigun = player.GetMedigun()) != nullptr)
		{
			strcat_s(iconPath, "_");
			strcat_s(iconPath, TF_RESIST_TYPE_NAMES[(int)s_MedigunResistType.GetValue(medigun)]);
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

	if (auto chargebar = dynamic_cast<vgui::ContinuousProgressBar*>(FindChildByName(playerVPanel, isRedTeam ? "WeaponChargeRed" : "WeaponChargeBlue")))
		chargebar->SetVisible(shouldShowInfo);

	if (auto icon = dynamic_cast<vgui::ImagePanel*>(FindChildByName(playerVPanel, isRedTeam ? "WeaponChargeIconRed" : "WeaponChargeIconBlue")))
		icon->SetImage(iconPath);
}
#pragma warning(pop)

void HUDHacking::UpdateClassChangeAnimations(vgui::VPANEL playerVPanel, vgui::EditablePanel* playerPanel, Player& player)
{
	auto animController = GetAnimationController();
	if (!animController)
		return;

	const auto team = player.GetTeam();

	if (player.GetState<PlayerState>().WasClassChangedThisFrame())
	{
		auto startAnimSequence = HookManager::GetRawFunc<HookFunc::vgui_AnimationController_StartAnimationSequence>();
		startAnimSequence(animController, playerPanel, player.GetTeam() == TFTeam::Red ? "PlayerPanel_ClassChangedRed" : "PlayerPanel_ClassChangedBlue", false);
	}
}

void HUDHacking::CheckItemForCharge(Player& player, const IClientNetworkable& playerNetworkable, const IClientNetworkable& item,
	int& bestPriority, ChargeBarType& type, float& charge) const
{
	const auto itemID = ItemSchema::GetModule()->GetBaseItemID(Entities::GetItemDefinitionIndex(&item));

	for (size_t i = 0; i < std::size(s_ChargeBarInfo); i++)
	{
		const auto& info = s_ChargeBarInfo[i];
		if (itemID != info.m_ItemID)
			continue;

		const auto priority = m_ChargeBarCvars[i]->m_PriorityCvar.GetInt();
		if (priority <= bestPriority)
			continue;

		bestPriority = priority;
		type = (ChargeBarType)i;
		charge = GetChargeMeter(info, player, playerNetworkable, item);
	}
}

bool HUDHacking::GetChargeBarData(Player& player, ChargeBarType& type, float& charge) const
{
	if (false)
	{
		int i = 0;

		static const RecvTable* excludeTables[] =
		{
			Entities::FindRecvTable("DT_BaseFlex"),
			Entities::FindRecvTable("DT_AttributeList"),
			Entities::FindRecvTable("DT_EconEntity"),
			Entities::FindRecvTable("m_chAreaBits"),
			Entities::FindRecvTable("m_chAreaPortalBits"),
			Entities::FindRecvTable("DT_Local"),
			Entities::FindRecvTable("m_iAmmo"),
		};

		for (const auto& prop : EntityOffsetIterable<int>(player.GetEntity()->GetClientClass()->m_pRecvTable, std::begin(excludeTables), std::end(excludeTables)))
			engine->Con_NPrintf(i++, "%s: %i", prop.first.c_str(), prop.second.GetValue(player.GetEntity()));
	}

	IClientNetworkable* playerNetworkable;
	if (auto playerEnt = player.GetEntity())
		playerNetworkable = playerEnt->GetClientNetworkable();
	else
		return false;

	int bestPriority = 0;
	for (int iWep = 0; iWep < MAX_WEAPONS; iWep++)
	{
		if (auto weapon = player.GetWeapon(iWep))
			CheckItemForCharge(player, *playerNetworkable, *weapon, bestPriority, type, charge);
	}

	// Demoman shields and razorbacks are weird and don't go in the m_hMyWeapons array >.<
	for (int iWearable = 0; iWearable < 4; iWearable++)
	{
		C_BaseEntity* item = HookManager::GetRawFunc<HookFunc::C_TFPlayer_GetEntityForLoadoutSlot>()((C_TFPlayer*)player.GetEntity(), iWearable, true);
		if (!item)
			continue;

		if (s_WearableShieldType.Match(item) || s_RazorbackType.Match(item))
			CheckItemForCharge(player, *playerNetworkable, *item, bestPriority, type, charge);

		continue;
	}

	return bestPriority > 0;
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
		constexpr ProgressBarName(const char* name, TFTeam team, bool overheal, bool inverse) :
			m_Name(name), m_Team(team), m_Overheal(overheal), m_Inverse(inverse)
		{
		}

		const char* m_Name;
		TFTeam m_Team;
		bool m_Overheal;
		bool m_Inverse;
	};

	static constexpr ProgressBarName s_ProgressBars[] =
	{
		ProgressBarName("PlayerHealthRed", TFTeam::Red, false, false),
		ProgressBarName("PlayerHealthInverseRed", TFTeam::Red, false, true),
		ProgressBarName("PlayerHealthBlue", TFTeam::Blue, false, false),
		ProgressBarName("PlayerHealthInverseBlue", TFTeam::Blue, false, true),

		ProgressBarName("PlayerHealthOverhealRed", TFTeam::Red, true, false),
		ProgressBarName("PlayerHealthInverseOverhealRed", TFTeam::Red, true, true),
		ProgressBarName("PlayerHealthOverhealBlue", TFTeam::Blue, true, false),
		ProgressBarName("PlayerHealthInverseOverhealBlue", TFTeam::Blue, true, true)
	};

	const auto team = player.GetTeam();

	// Show/hide progress bars
	for (const auto& bar : s_ProgressBars)
	{
		// Only update the progress bars for the appropriate team
		if (bar.m_Team != team)
			continue;

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

vgui::Panel* HUDHacking::FindChildByNameOverride(vgui::Panel* pThis, const char* name, bool recurseDown)
{
	m_FindChildByNameHook.SetState(Hooking::HookAction::SUPERCEDE);

	while (!strncmp(name, "../", 3))
	{
		name += 3;
		pThis = pThis->GetParent();

		if (!pThis)
			return nullptr;
	}

	return m_FindChildByNameHook.GetOriginal()(pThis, name, recurseDown);
}

bool HUDHacking::PlayerState::WasClassChangedThisFrame() const
{
	return Interfaces::GetEngineTool()->HostFrameCount() == m_LastClassChangedFrame;
}

float HUDHacking::PlayerState::GetCleanersCarbineCharge() const
{
	if (m_CleanersCarbineEmptyTime > 0)
		return std::clamp<float>((m_CleanersCarbineEmptyTime - Interfaces::GetEngineTool()->ClientTime()) / CLEANERS_CARBINE_DURATION, 0, 1) * 100;
	else
		return m_LastCleanersCarbineCharge;
}

void HUDHacking::PlayerState::UpdateInternal(bool tickUpdate, bool frameUpdate)
{
	if (tickUpdate)
	{
		UpdateClass();
		UpdateCleanersCarbineMeter();
	}
}

void HUDHacking::PlayerState::UpdateClass()
{
	auto playerClass = GetPlayer().GetClass();

	// Update last class changed frame
	if (playerClass != m_LastClassChangedClass)
		m_LastClassChangedFrame = Interfaces::GetEngineTool()->HostFrameCount();

	m_LastClassChangedClass = playerClass;
}

void HUDHacking::PlayerState::UpdateCleanersCarbineMeter()
{
	float newCharge = -1;
	if (GetPlayer().IsAlive())
	{
		for (int i = 0; i < MAX_WEAPONS; i++)
		{
			if (auto charge = s_CleanersCarbineCharge.TryGetValue(GetPlayer().GetWeapon(i)); charge)
				newCharge = *charge;
		}
	}

	if (newCharge == 0 && m_LastCleanersCarbineCharge == 100 && GetPlayer().CheckCondition(TFCond_CritCola))
	{
		// We just started using our meter
		m_CleanersCarbineEmptyTime = Interfaces::GetEngineTool()->ClientTime() + CLEANERS_CARBINE_DURATION;
	}
	else if (newCharge < 0 || !GetPlayer().CheckCondition(TFCond_CritCola))
	{
		// We're dead or we changed class or something. Snap meter to empty.
		m_CleanersCarbineEmptyTime = 0;
	}

	m_LastCleanersCarbineCharge = newCharge;
}

template<typename... Args>
std::unique_ptr<char[]> HUDHacking::WeaponChargeBarCvarData::FormatAndAlloc(const char* fmt, const Args&... args)
{
	char buf[256];
	auto length = sprintf_s(buf, fmt, args...) + 1;

	auto retVal = std::make_unique<char[]>(length);
	memcpy(retVal.get(), buf, length);
	return retVal;
}

HUDHacking::WeaponChargeBarCvarData::WeaponChargeBarCvarData(const ChargeBarInfo& info) :
	m_PriorityCvarName(FormatAndAlloc("ce_hud_chargebars_priority_%s", info.m_Name)),
	m_PriorityCvarHelpText(FormatAndAlloc("Priority of the charge bar for %s when competing with other enabled chargebars. 0 to disable showing this charge bar.", info.m_HelpText)),
	m_PriorityCvarDefault(FormatAndAlloc("%i", info.m_DefaultPriority)),

	m_TextCvarName(FormatAndAlloc("ce_hud_chargebars_text_%s", info.m_Name)),
	m_TextCvarHelpText(FormatAndAlloc("Text to display on the hud when the chargebar for %s is showing.", info.m_HelpText)),

	m_PriorityCvar(m_PriorityCvarName.get(), m_PriorityCvarDefault.get(), FCVAR_NONE, m_PriorityCvarHelpText.get()),
	m_TextCvar(m_TextCvarName.get(), info.m_DisplayText, FCVAR_NONE, m_TextCvarHelpText.get())
{
}
