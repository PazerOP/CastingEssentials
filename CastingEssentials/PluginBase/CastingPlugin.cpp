#include "Plugin.h"

#include "Interfaces.h"
#include "Modules.h"
#include "HookManager.h"
#include "Player.h"

#include "Modules/Antifreeze.h"
#include "Modules/AutoCameras.h"
#include "Modules/CameraAutoSwitch.h"
#include "Modules/CameraSmooths.h"
#include "Modules/CameraState.h"
#include "Modules/CameraTools.h"
#include "Modules/ConsoleTools.h"
#include "Modules/FOVOverride.h"
#include "Modules/FreezeInfo.h"
#include "Modules/Graphics.h"
#include "Modules/HitEvents.h"
#include "Modules/HUDHacking.h"
#include "Modules/IngameTeamScores.h"
#include "Modules/ItemSchema.h"
#include "Modules/Killfeed.h"
#include "Modules/Killstreaks.h"
#include "Modules/LoadoutIcons.h"
#include "Modules/LocalPlayer.h"
#include "Modules/MapConfigs.h"
#include "Modules/MedigunInfo.h"
#include "Modules/PlayerAliases.h"
#include "Modules/ProjectileOutlines.h"
#include "Modules/SteamTools.h"
#include "Modules/TeamNames.h"
#include "Modules/ClientTools.h"
#include "Modules/WeaponTools.h"

const char* const PLUGIN_VERSION_ID = "r20 beta";
const char* const PLUGIN_FULL_VERSION = strdup(strprintf("%s %s", PLUGIN_NAME, PLUGIN_VERSION_ID).c_str());

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
	const auto startTime = std::chrono::high_resolution_clock::now();

	Msg("Hello from %s!\n", PLUGIN_FULL_VERSION);

#ifdef DEBUG
	//PluginMsg("_CrtCheckMemory() result: %i\n", _CrtCheckMemory());
	//_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
#endif

	Interfaces::Load(interfaceFactory);
	HookManager::Load();
	Player::Load();
	Modules().Init();

	Modules().RegisterAndLoadModule<HUDHacking>("Evil HUD Modifications");

	// CameraTools and CameraSmooths depend on CameraState
	Modules().RegisterAndLoadModule<CameraState>("Camera State");
	Modules().RegisterAndLoadModule<CameraTools>("Camera Tools");
	Modules().RegisterAndLoadModule<CameraSmooths>("Camera Smooths");

	Modules().RegisterAndLoadModule<AntiFreeze>("HUD Antifreeze");
	Modules().RegisterAndLoadModule<AutoCameras>("AutoCameras");
	Modules().RegisterAndLoadModule<CameraAutoSwitch>("Camera Auto-Switch");
	Modules().RegisterAndLoadModule<ConsoleTools>("Console Tools");
	Modules().RegisterAndLoadModule<FOVOverride>("FOV Override");
	Modules().RegisterAndLoadModule<FreezeInfo>("Freeze Info");
	Modules().RegisterAndLoadModule<Graphics>("Graphics Enhancements");
	Modules().RegisterAndLoadModule<HitEvents>("Player Hit Events");
	Modules().RegisterAndLoadModule<IngameTeamScores>("Ingame Team Scores");
	Modules().RegisterAndLoadModule<ItemSchema>("Item Schema");
	Modules().RegisterAndLoadModule<Killfeed>("Killfeed Fixes");
	Modules().RegisterAndLoadModule<Killstreaks>("Killstreaks");
	Modules().RegisterAndLoadModule<LoadoutIcons>("Loadout Icons");
	Modules().RegisterAndLoadModule<LocalPlayer>("Local Player");
	Modules().RegisterAndLoadModule<MapConfigs>("Map Configs");
	Modules().RegisterAndLoadModule<MedigunInfo>("Medigun Info");
	Modules().RegisterAndLoadModule<PlayerAliases>("Player Aliases");
	Modules().RegisterAndLoadModule<ProjectileOutlines>("Projectile Outlines");
	Modules().RegisterAndLoadModule<SteamTools>("Steam Tools");
	Modules().RegisterAndLoadModule<TeamNames>("Team Names");
	Modules().RegisterAndLoadModule<ClientTools>("Client Tools");
	Modules().RegisterAndLoadModule<WeaponTools>("Weapon Tools");

	ConVar_Register();

	const auto endTime = std::chrono::high_resolution_clock::now();
	const auto delta = std::chrono::duration<float>(endTime - startTime);
	PluginMsg("Finished loading in %1.2f seconds.\n", delta.count());

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
