#include "Plugin.h"
#include <materialsystem/imaterialsystem.h>
#include <tier1/tier1.h>
#include <tier2/tier2.h>
#include <tier3/tier3.h>

#include "Interfaces.h"
#include "Modules.h"
#include "HookManager.h"
#include "Player.h"

#include "Modules/CameraAutoSwitch.h"
#include "Modules/CameraTools.h"
#include "Modules/ConsoleTools.h"
#include "Modules/FOVOverride.h"
#include "Modules/Killstreaks.h"
#include "Modules/LocalPlayer.h"
#include "Modules/MedigunInfo.h"
#include "Modules/PlayerAliases.h"
#include "Modules/ProjectileOutlines.h"
#include "Modules/SteamTools.h"
#include "Modules/TeamNames.h"

class CastingPlugin final : public Plugin
{
public:
	CastingPlugin() = default;
	virtual ~CastingPlugin() { }

	bool Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory) override;
	void Unload() override;
	const char* GetPluginDescription() override { return "CastingEssentials"; }
};

static CastingPlugin s_CastingPlugin;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CastingPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, s_CastingPlugin);

bool CastingPlugin::Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory)
{
	Msg("Hello from CastingEssentials!\n");

#ifdef DEBUG
	//PluginMsg("_CrtCheckMemory() result: %i\n", _CrtCheckMemory());
	//_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
#endif
	
	Interfaces::Load(interfaceFactory);
	HookManager::Load();
	Modules().Init();

	//auto test = new vgui::Label(nullptr, "testPanel", "Hello World");

	Modules().RegisterAndLoadModule<CameraAutoSwitch>("Camera Auto-Switch");
	Modules().RegisterAndLoadModule<CameraTools>("Camera Tools");
	Modules().RegisterAndLoadModule<ConsoleTools>("Console Tools");
	Modules().RegisterAndLoadModule<FOVOverride>("FOV Override");
	Modules().RegisterAndLoadModule<Killstreaks>("Killstreaks");
	Modules().RegisterAndLoadModule<LocalPlayer>("Local Player");
	Modules().RegisterAndLoadModule<MedigunInfo>("Medigun Info");
	Modules().RegisterAndLoadModule<PlayerAliases>("Player Aliases");
	Modules().RegisterAndLoadModule<ProjectileOutlines>("Projectile Outlines");
	Modules().RegisterAndLoadModule<SteamTools>("Steam Tools");
	Modules().RegisterAndLoadModule<TeamNames>("Team Names");

	ConVar_Register();

	PluginMsg("Finished loading!\n");

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