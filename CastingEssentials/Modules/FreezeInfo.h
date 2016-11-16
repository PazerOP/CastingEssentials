#pragma once

#include "PluginBase/Modules.h"

class ConCommand;
class ConVar;
class IConVar;

class FreezeInfo final : public Module
{
public:
	FreezeInfo();
	~FreezeInfo();

	static bool CheckDependencies();

	static FreezeInfo* GetModule() { return Modules().GetModule<FreezeInfo>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<FreezeInfo>().c_str(); }

private:
	void OnTick(bool inGame) override;

	class Panel;
	std::unique_ptr<Panel> m_Panel;

	void PostEntityPacketReceivedHook();
	int m_PostEntityPacketReceivedHook;

	ConVar *enabled;
	ConCommand *reload_settings;
	ConVar *threshold;
	void ChangeThreshold(IConVar *var, const char *pOldValue, float flOldValue);
	void ReloadSettings();
	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);
};