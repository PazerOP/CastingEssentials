#include "Plugin.h"
#include <materialsystem/imaterialsystem.h>
#include <tier1/tier1.h>
#include <tier2/tier2.h>
#include <tier3/tier3.h>

#include "Interfaces.h"
#include "Modules.h"
#include "HookManager.h"
#include "Player.h"

#include "Modules/Antifreeze.h"
#include "Modules/CameraAutoSwitch.h"
#include "Modules/CameraSmooths.h"
#include "Modules/CameraState.h"
#include "Modules/CameraTools.h"
#include "Modules/ConsoleTools.h"
#include "Modules/FOVOverride.h"
#include "Modules/FreezeInfo.h"
#include "Modules/Killstreaks.h"
#include "Modules/LocalPlayer.h"
#include "Modules/MapConfigs.h"
#include "Modules/MapFlythroughs.h"
#include "Modules/MedigunInfo.h"
#include "Modules/PlayerAliases.h"
#include "Modules/ProjectileOutlines.h"
#include "Modules/SteamTools.h"
#include "Modules/TeamNames.h"

const char* PLUGIN_VERSION_ID = "r9b1";
const char* PLUGIN_FULL_VERSION = strdup(strprintf("%s %s", PLUGIN_NAME, PLUGIN_VERSION_ID).c_str());

class CastingPlugin final : public Plugin
{
public:
	CastingPlugin() = default;
	virtual ~CastingPlugin() { }

	bool Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory) override;
	void Unload() override;
	const char* GetPluginDescription() override { return PLUGIN_FULL_VERSION; }
};

static CastingPlugin s_CastingPlugin;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CastingPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, s_CastingPlugin);

bool CastingPlugin::Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory)
{
	Msg("Hello from %s!\n", PLUGIN_FULL_VERSION);

#ifdef DEBUG
	//PluginMsg("_CrtCheckMemory() result: %i\n", _CrtCheckMemory());
	//_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
#endif
	
	Interfaces::Load(interfaceFactory);
	HookManager::Load();
	Modules().Init();

	// CameraTools and CameraSmooths depend on CameraState
	Modules().RegisterAndLoadModule<CameraState>("Camera State");
	Modules().RegisterAndLoadModule<CameraTools>("Camera Tools");
	Modules().RegisterAndLoadModule<CameraSmooths>("Camera Smooths");

	Modules().RegisterAndLoadModule<AntiFreeze>("HUD Antifreeze");
	Modules().RegisterAndLoadModule<CameraAutoSwitch>("Camera Auto-Switch");
	Modules().RegisterAndLoadModule<ConsoleTools>("Console Tools");
	Modules().RegisterAndLoadModule<FOVOverride>("FOV Override");
	Modules().RegisterAndLoadModule<FreezeInfo>("Freeze Info");
	Modules().RegisterAndLoadModule<Killstreaks>("Killstreaks");
	Modules().RegisterAndLoadModule<LocalPlayer>("Local Player");
	Modules().RegisterAndLoadModule<MapConfigs>("Map Configs");
	Modules().RegisterAndLoadModule<MapFlythroughs>("Map Flythroughs");
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