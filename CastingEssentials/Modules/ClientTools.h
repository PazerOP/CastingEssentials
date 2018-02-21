#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>

class ClientTools : public Module<ClientTools>
{
public:
	ClientTools();

	static bool CheckDependencies();

private:
	void UpdateWindowTitle(const char* oldval);

	ConVar ce_clienttools_windowtitle;

	std::string m_OriginalTitle;
};
