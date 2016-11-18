#pragma once

#include "PluginBase/Modules.h"

class ConCommand;
class ConVar;
class IConVar;

class MedigunInfo : public Module
{
public:
	MedigunInfo();

	static MedigunInfo* GetModule() { return Modules().GetModule<MedigunInfo>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<MedigunInfo>().c_str(); }

	static bool CheckDependencies();
private:
	class MainPanel;
	std::unique_ptr<MainPanel> m_MainPanel;
	class MedigunPanel;

	void OnTick(bool inGame) override;

	ConVar *enabled;
	ConCommand *reload_settings;
	void ReloadSettings();
};