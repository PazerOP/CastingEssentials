#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>

class FreezeInfo final : public Module<FreezeInfo>
{
public:
	FreezeInfo();
	~FreezeInfo();

	static bool CheckDependencies();

private:
	void OnTick(bool inGame) override;

	class Panel;
	std::unique_ptr<Panel> m_Panel;

	void PostEntityPacketReceivedHook();
	int m_PostEntityPacketReceivedHook;

	ConVar ce_freezeinfo_enabled;
	ConVar ce_freezeinfo_threshold;
	ConCommand ce_freezeinfo_reload_settings;

	void ChangeThreshold(IConVar *var, const char *pOldValue, float flOldValue);
	void ReloadSettings();
	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);
};