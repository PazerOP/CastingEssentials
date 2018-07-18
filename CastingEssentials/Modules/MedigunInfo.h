#pragma once

#include "PluginBase/EntityOffset.h"
#include "PluginBase/Modules.h"

#include <convar.h>

enum class TFMedigun;
enum class TFResistType;
enum class TFTeam;

namespace vgui
{
	typedef unsigned int VPANEL;
	class EditablePanel;
}

class MedigunInfo : public Module<MedigunInfo>
{
public:
	MedigunInfo();

	static bool CheckDependencies();

protected:
	void LevelShutdown() override;

private:
	struct Data
	{
		bool m_Alive;
		float m_Charge;
		TFMedigun m_Type;
		bool m_Popped;
		TFResistType m_ResistType;
		TFTeam m_Team;
	};

	void CollectMedigunData();
	std::map<int, Data> m_MedigunPanelData;

	class MainPanel;
	std::unique_ptr<MainPanel> m_MainPanel;
	class MedigunPanel;

	void OnTick(bool inGame) override;

	void UpdateEmbeddedPanels();
	void UpdateEmbeddedPanel(vgui::EditablePanel* playerPanel);

	ConVar ce_mediguninfo_separate_enabled;
	ConCommand ce_mediguninfo_separate_reload;

	ConVar ce_mediguninfo_embedded_enabled;
	ConVar ce_mediguninfo_embedded_medigun_text;
	ConVar ce_mediguninfo_embedded_kritzkrieg_text;
	ConVar ce_mediguninfo_embedded_quickfix_text;
	ConVar ce_mediguninfo_embedded_vaccinator_text;
	ConVar ce_mediguninfo_embedded_dead_text;

	static EntityOffset<bool> s_ChargeRelease;
	static EntityOffset<TFResistType> s_ChargeResistType;
	static EntityOffset<float> s_ChargeLevel;

	void ReloadSettings();

	static constexpr const char* EMBEDDED_ICON_RED = "MedigunIconRed";
	static constexpr const char* EMBEDDED_ICON_BLUE = "MedigunIconBlue";
	static constexpr const char* EMBEDDED_PROGRESS_RED = "MedigunChargeRed";
	static constexpr const char* EMBEDDED_PROGRESS_BLUE = "MedigunChargeBlue";
};