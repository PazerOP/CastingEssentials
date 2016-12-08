#pragma once

class C_BaseEntity;
class ConVar;
class IConVar;

#include <ehandle.h>
#include "PluginBase/Modules.h"

#include <memory>

class Killstreaks : public Module
{
public:
	Killstreaks();

	static bool CheckDependencies();

	static Killstreaks* GetModule() { return Modules().GetModule<Killstreaks>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<Killstreaks>().c_str(); }

private:
	class Panel;
	std::unique_ptr<Panel> panel;

	ConVar *enabled;
	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);
};