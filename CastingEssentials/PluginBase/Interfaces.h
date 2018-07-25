#pragma once

#include "interface.h"
#include <utlvector.h>

class C_BasePlayer;
class C_GameRules;
class C_HLTVCamera;
struct cmdalias_t;
class HLTVCameraOverride;
class CSteamAPIContext;
class IBaseClientDLL;
class IClientEngineTools;
class IClientEntityList;
class IClientMode;
class IClientRenderTargets;
class IEngineTool;
class IGameEventManager2;
class IGameSystem;
class IGameSystemPerFrame;
class IPrediction;
class IVEngineClient;
class IVModelInfoClient;
class IVRenderView;
class IFileSystem;
class IVDebugOverlay;
class IEngineTrace;
class ISpatialPartition;
class IMaterialSystem;
class IShaderAPI;
class IClientLeafSystem;

class Interfaces final
{
public:
	Interfaces() = delete;
	~Interfaces() = delete;

	static void Load(CreateInterfaceFn factory);
	static void Unload();

	// #include <cdll_int.h>
	static IBaseClientDLL* GetClientDLL() { return pClientDLL; }

	// #include <toolframework/iclientenginetools.h>
	static IClientEngineTools* GetClientEngineTools() { return pClientEngineTools; }

	// #include <client/cliententitylist.h>
	static IClientEntityList* GetClientEntityList() { return pClientEntityList; }

	// #include <cdll_int.h>
	static IVEngineClient* GetEngineClient() { return pEngineClient; }

	// #include <toolframework/ienginetool.h>
	static IEngineTool* GetEngineTool() { return pEngineTool; }

	// #include <igameevents.h>
	static IGameEventManager2* GetGameEventManager() { return pGameEventManager; }

	// #include <iprediction.h>
	static IPrediction* GetPrediction() { return pPrediction; }
	static IVModelInfoClient* GetModelInfoClient() { return pModelInfoClient; }

	// #include <imaterialsystem.h>
	static IMaterialSystem* GetMaterialSystem() { return pMaterialSystem; }

	// #include <shaderapi/ishaderapi.h>
	static IShaderAPI* GetShaderAPI() { return s_ShaderAPI; }

	// #include <ivrenderview.h>
	static IVRenderView* GetRenderView() { return pRenderView; }

	// #include <steam/steam_api.h>
	static CSteamAPIContext* GetSteamAPIContext() { return pSteamAPIContext; }

	// #include <filesystem.h>
	static IFileSystem* GetFileSystem() { return s_FileSystem; }

	static bool AreSteamLibrariesAvailable() { return steamLibrariesAvailable; }
	static bool AreVguiLibrariesAvailable() { return vguiLibrariesAvailable; }

	// #include <client/iclientmode.h>
	static IClientMode *GetClientMode();

	// #include "Misc/HLTVCameraHack.h"
	static HLTVCameraOverride *GetHLTVCamera();

	static C_BasePlayer*& GetLocalPlayer();

	// #include <engine/ivdebugoverlay.h>
	static IVDebugOverlay* GetDebugOverlay() { return s_DebugOverlay; }

	// #include <engine/IEngineTrace.h>
	static IEngineTrace* GetEngineTrace() { return s_EngineTrace; }

	// #include <ispatialpartition.h>
	static ISpatialPartition* GetSpatialPartition() { return s_SpatialPartition; }

	// #include <clientleafsystem.h>
	static IClientLeafSystem* GetClientLeafSystem() { return s_ClientLeafSystem; }

	// #include "Misc/CmdAlias.h"
	static cmdalias_t** GetCmdAliases();

	// #include <shared/gamerules.h>
	static C_GameRules* GetGameRules();

	// #include <shared/igamesystem.h>
	static CUtlVector<IGameSystem*>* GetGameSystems();

	// #include <shared/igamesystem.h>
	static CUtlVector<IGameSystemPerFrame*>* GetGameSystemsPerFrame();

	// #include <game/client/iclientrendertargets.h>
	static IClientRenderTargets* GetClientRenderTargets() { return s_ClientRenderTargets; }

private:
	static IBaseClientDLL *pClientDLL;
	static IClientEngineTools *pClientEngineTools;
	static IClientEntityList *pClientEntityList;
	static IVEngineClient *pEngineClient;
	static IEngineTool *pEngineTool;
	static IGameEventManager2 *pGameEventManager;
	static IVModelInfoClient *pModelInfoClient;
	static IPrediction *pPrediction;
	static IMaterialSystem* pMaterialSystem;
	static IShaderAPI* s_ShaderAPI;
	static IVRenderView *pRenderView;
	static CSteamAPIContext *pSteamAPIContext;
	static IFileSystem* s_FileSystem;
	static IVDebugOverlay* s_DebugOverlay;
	static IEngineTrace* s_EngineTrace;
	static ISpatialPartition* s_SpatialPartition;
	static IClientLeafSystem* s_ClientLeafSystem;
	static IClientRenderTargets* s_ClientRenderTargets;

	static bool steamLibrariesAvailable;
	static bool vguiLibrariesAvailable;

	static IClientMode* s_ClientMode;
	static C_HLTVCamera** s_HLTVCamera;
};