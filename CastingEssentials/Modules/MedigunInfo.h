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
	MainPanel *mainPanel;
	class MedigunPanel;

	ConVar *enabled;
	ConCommand *reload_settings;
	void ReloadSettings();
	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);
};