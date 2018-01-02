#pragma once
#include "PluginBase/modules.h"

class ConCommand;
class ConVar;
class IConVar;

class AntiFreeze : public Module<AntiFreeze>
{
public:
	AntiFreeze();

	static bool CheckDependencies();

private:
	class Panel;
	std::unique_ptr<Panel> m_Panel;

	void OnTick(bool inGame) override;

	ConVar *enabled;
	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);
};