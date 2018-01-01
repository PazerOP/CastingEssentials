#pragma once

#include "PluginBase/Modules.h"

enum class TFMedigun;
enum class TFResistType;
enum class TFTeam;
class ConCommand;
class ConVar;
class IConVar;

namespace vgui
{
	typedef unsigned int VPANEL;
	class EditablePanel;
}

class MedigunInfo : public Module
{
public:
	MedigunInfo();

	static MedigunInfo* GetModule() { return Modules().GetModule<MedigunInfo>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<MedigunInfo>().c_str(); }

	static bool CheckDependencies();
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

	class MainPanel;
	std::unique_ptr<MainPanel> m_MainPanel;
	class MedigunPanel;

	void OnTick(bool inGame) override;

	void UpdateEmbeddedPanels();
	void UpdateEmbeddedPanel(vgui::EditablePanel* playerPanel);

	ConVar *enabled;
	ConCommand *reload_settings;
	void ReloadSettings();

	static constexpr const char* EMBEDDED_ICON_RED = "MedigunIconRed";
	static constexpr const char* EMBEDDED_ICON_BLUE = "MedigunIconBlue";
};