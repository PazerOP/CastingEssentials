#pragma once

class C_BaseEntity;
class ConVar;
class IConVar;

#include <ehandle.h>
#include "PluginBase/Modules.h"

#include <memory>

class Killstreaks : public Module<Killstreaks>
{
public:
	Killstreaks();

	static bool CheckDependencies();

private:
	class Panel;
	std::unique_ptr<Panel> panel;

	ConVar *enabled;
	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);
};