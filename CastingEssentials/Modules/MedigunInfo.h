#pragma once

#include "PluginBase/EntityOffset.h"
#include "PluginBase/Modules.h"

#include <convar.h>
#include <map>

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
	static constexpr __forceinline const char* GetModuleName() { return "Medigun Info"; }

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

	ConVar ce_mediguninfo_separate_enabled;
	ConCommand ce_mediguninfo_separate_reload;

	static EntityOffset<bool> s_ChargeRelease;
	static EntityOffset<TFResistType> s_ChargeResistType;
	static EntityOffset<float> s_ChargeLevel;

	void ReloadSettings();
};