#include "Plugin.h"

#include "Interfaces.h"
#include "Modules.h"
#include "HookManager.h"
#include "Player.h"
#include "Entities.h"

#include <convar.h>

const char* const PLUGIN_VERSION_ID = "r21";
const char* const PLUGIN_FULL_VERSION = strdup(strprintf("%s %s", PLUGIN_NAME, PLUGIN_VERSION_ID).c_str());

class CastingPlugin final : public Plugin
{
public:
	CastingPlugin();

	bool Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory) override;
	void Unload() override;
	const char* GetPluginDescription() override { return PLUGIN_FULL_VERSION; }

private:
	static void PrintLicence();
	ConCommand ce_license;
};

static CastingPlugin s_CastingPlugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CastingPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, s_CastingPlugin);

CastingPlugin::CastingPlugin() :
	ce_license("ce_license", PrintLicence, "CastingEssentials license information")
{
}

bool CastingPlugin::Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory)
{
	const auto startTime = std::chrono::high_resolution_clock::now();

	Msg("Hello from %s!\n", PLUGIN_FULL_VERSION);

	Interfaces::Load(interfaceFactory);
	HookManager::Load();
	Entities::Load();
	Player::Load();

	Modules().Init();
	Modules().LoadAll();

	ConVar_Register();

	const auto endTime = std::chrono::high_resolution_clock::now();
	const auto delta = std::chrono::duration<float>(endTime - startTime);
	PluginMsg("Finished loading %d modules in %1.2f seconds.\n", Modules().size(), delta.count());

	return true;
}

void CastingPlugin::Unload()
{
#ifdef DEBUG
	//PluginMsg("_CrtCheckMemory() result: %i\n", _CrtCheckMemory());
#endif
	PluginMsg("Unloading plugin...\n");

	Player::Unload();
	ConVar_Unregister();
	Modules().UnloadAllModules();
	HookManager::Unload();
	Interfaces::Unload();

	PluginMsg("Finished unloading!\n");
}

void CastingPlugin::PrintLicence()
{
	static constexpr const char LICENSE_TEXT[] =
		R"LICENSE(BSD 2-Clause License

Copyright (c) 2018, PazerOP
Portions copyright (c) fwdcp
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.)LICENSE";

	Msg("%s\n", LICENSE_TEXT);

}