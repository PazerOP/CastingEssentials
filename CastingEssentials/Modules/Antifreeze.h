#pragma once
#include "PluginBase/modules.h"

#include <convar.h>

class ConCommand;
class IConVar;

class AntiFreeze : public Module<AntiFreeze>
{
public:
	AntiFreeze();

	static bool CheckDependencies();
	static constexpr __forceinline const char* GetModuleName() { return "HUD Antifreeze"; }

private:
	class Panel;
	std::unique_ptr<Panel> m_Panel;

	void OnTick(bool inGame) override;

	ConVar ce_antifreeze_enabled;
	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);
};