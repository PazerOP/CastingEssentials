#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>

class ClientTools : public Module<ClientTools>
{
public:
	static ClientTools* GetModule() { return Modules().GetModule<ClientTools>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<ClientTools>().c_str(); }

	static bool CheckDependencies();
private:

	void UpdateWindowTitle(const char* oldval);
	ConVar cv_windowtitle{ "ce_clienttools_windowtitle", "", FCVAR_NONE, "Overrides the game window title", [](IConVar* var, const char* oldval, float foldval) { GetModule()->UpdateWindowTitle(oldval); } };
	std::string origtitle;
};
