#pragma once
#include "PluginBase/modules.h"

class ConCommand;
class ConVar;
class IConVar;

class AntiFreeze : public Module
{
public:
	AntiFreeze();

	static bool CheckDependencies();

	static AntiFreeze* GetModule() { return Modules().GetModule<AntiFreeze>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<AntiFreeze>().c_str(); }

private:
	class Panel;
	std::unique_ptr<Panel> m_Panel;

	void OnTick(bool inGame) override;

	ConVar *enabled;
	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);
};