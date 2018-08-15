#pragma once
#include "PluginBase/EntityOffset.h"
#include "PluginBase/Hook.h"
#include "PluginBase/Modules.h"
#include "PluginBase/PlayerStateBase.h"

#include <convar.h>

#include <optional>

class Player;
enum class TFClassType;

namespace vgui
{
	typedef unsigned int VPANEL;
	class AnimationController;
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
	static constexpr __forceinline const char* GetModuleName() { return "Evil HUD Modifications"; }

	static vgui::VPANEL GetSpecGUI();
	static Player* GetPlayerFromPanel(vgui::EditablePanel* playerPanel);

	// Like normal FindChildByName, but doesn't care about the fact we're in a different module.
	static vgui::Panel* FindChildByName(vgui::VPANEL rootPanel, const char* name, bool recursive = false);

private:
	ConVar ce_hud_forward_playerpanel_border;
	ConVar ce_hud_player_health_progressbars;
	ConVar ce_hud_player_status_effects;
	ConVar ce_hud_player_status_effects_debug;
	ConVar ce_hud_progressbar_directions;
	ConVar ce_hud_find_parent_elements;

	ConVar ce_hud_class_change_animations;

	///////////////////////
	// Charge bars begin //
	///////////////////////
	ConVar ce_hud_chargebars_enabled;

	enum class ChargeBarType
	{
		SodaPopper,
		BabyFacesBlaster,
		Bonk,
		CritACola,
		MadMilk,
		Cleaver,
		Sandman,
		WrapAssassin,

		BannerBattalions,
		BannerBuff,
		BannerConch,

		Phlog,
		GasPasser,
		Jetpack,

		ShieldTarge,
		ShieldSplendid,
		ShieldTide,

		Sandvich,
		DalokohsBar,
		SteakSandvich,
		Banana,

		MedigunUber,
		MedigunKritz,
		MedigunQuickfix,
		MedigunVaccinator,

		HitmansHeatmaker,
		Jarate,
		Razorback,
		CleanersCarbine,

		Spycicle,
		InvisWatch,
		CloakAndDagger,
		DeadRinger,

		COUNT,
	};

	enum class MeterType
	{
		Unknown,

		Rage,
		EnergyDrink,
		Hype,
		Charge,
		MedigunCharge,
		Cloak,
		WeaponEnergy,
		EffectBar,
		CleanersCarbine,
		Spycicle,

		ItemCharge0,
		ItemCharge1,
	};

	struct ChargeBarInfo
	{
	public:
		ChargeBarInfo(const char* name, int defaultPriority, const char* helpText,
			const char* displayText, int itemID, MeterType meter, float effectBarChargeTime = -1) :
			m_Name(name), m_DefaultPriority(defaultPriority), m_HelpText(helpText),
			m_DisplayText(displayText), m_ItemID(itemID), m_Meter(meter), m_EffectBarChargeTime(effectBarChargeTime)
		{
			Assert(meter != MeterType::EffectBar || m_EffectBarChargeTime > 0);
		}

		const char* m_Name;
		int m_DefaultPriority;
		const char* m_HelpText;
		const char* m_DisplayText;
		int m_ItemID;
		MeterType m_Meter;
		float m_EffectBarChargeTime;
	};

	struct WeaponChargeBarCvarData
	{
		WeaponChargeBarCvarData(const ChargeBarInfo& info);

	private:
		std::unique_ptr<char[]> m_PriorityCvarName;
		std::unique_ptr<char[]> m_PriorityCvarHelpText;
		std::unique_ptr<char[]> m_PriorityCvarDefault;
		std::unique_ptr<char[]> m_TextCvarName;
		std::unique_ptr<char[]> m_TextCvarHelpText;

		template<typename... Args> static std::unique_ptr<char[]> FormatAndAlloc(const char* fmt, const Args&... args);

	public:
		ConVar m_PriorityCvar;
		ConVar m_TextCvar;
	};
	std::optional<WeaponChargeBarCvarData> m_ChargeBarCvars[(int)ChargeBarType::COUNT];

	ConVar ce_hud_chargebars_debug;

	static const ChargeBarInfo s_ChargeBarInfo[(int)ChargeBarType::COUNT];

	/////////////////////
	// Charge bars end //
	/////////////////////

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

	class PlayerState : public PlayerStateBase
	{
	public:
		PlayerState(Player& player) : PlayerStateBase(player) {}
		bool WasClassChangedThisFrame() const;

		float GetCleanersCarbineCharge() const;

	protected:
		void UpdateInternal(bool tickUpdate, bool frameUpdate) override;

	private:
		void UpdateClass();
		void UpdateCleanersCarbineMeter();

		static constexpr float CLEANERS_CARBINE_DURATION = 8;
		float m_LastCleanersCarbineCharge = 0;
		float m_CleanersCarbineEmptyTime = 0;

		int m_LastClassChangedFrame;
		TFClassType m_LastClassChangedClass;
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
	void UpdateChargeBar(bool enabled, vgui::VPANEL playerVPanel, vgui::EditablePanel* playerPanel, Player& player);
	void UpdateClassChangeAnimations(vgui::VPANEL playerVPanel, vgui::EditablePanel* playerPanel, Player& player);

	bool GetChargeBarData(Player& player, ChargeBarType& type, float& charge) const;
	void CheckItemForCharge(Player& player, const IClientNetworkable& playerNetworkable, const IClientNetworkable& item,
		int& bestPriority, ChargeBarType& type, float& charge) const;
	float GetChargeMeter(const ChargeBarInfo& info, Player& player, const IClientNetworkable& playerNetworkable,
		const IClientNetworkable& weapon) const;

	Hook<HookFunc::vgui_ProgressBar_ApplySettings> m_ApplySettingsHook;
	static void ProgressBarApplySettingsHook(vgui::ProgressBar* pThis, KeyValues* pSettings);

	Hook<HookFunc::vgui_Panel_FindChildByName> m_FindChildByNameHook;
	vgui::Panel* FindChildByNameOverride(vgui::Panel* pThis, const char* name, bool recurseDown);

	static vgui::AnimationController* GetAnimationController();

	static EntityOffset<float> s_RageMeter;
	static EntityOffset<float> s_EnergyDrinkMeter;
	static EntityOffset<float> s_HypeMeter;
	static EntityOffset<float> s_ChargeMeter;
	static EntityOffset<float> s_CloakMeter;
	static EntityOffset<float> s_MedigunChargeMeter;
	static EntityOffset<float> s_WeaponEnergyMeter;
	static EntityOffset<float> s_WeaponEffectBarRegenTime;
	static EntityOffset<float> s_ItemChargeMeters[11];
	static EntityOffset<float> s_SpycicleRegenTime;
	static EntityOffset<float> s_SpycicleMeltTime;
	static EntityTypeChecker s_WearableShieldType;
	static EntityTypeChecker s_RazorbackType;
	static EntityOffset<float> s_CleanersCarbineCharge;
};