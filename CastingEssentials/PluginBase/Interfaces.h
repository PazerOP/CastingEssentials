#pragma once

#include "interface.h"
#include <memory>

class C_HLTVCamera;
class HLTVCameraOverride;
class CSteamAPIContext;
class IBaseClientDLL;
class IClientEngineTools;
class IClientEntityList;
class IClientMode;
class IEngineTool;
class IGameEventManager2;
class IPrediction;
class IVEngineClient;
class IVModelInfoClient;
class IVRenderView;
class IFileSystem;
class IVDebugOverlay;
class IEngineTrace;
class ISpatialPartition;

class Interfaces final
{
public:
	static void Load(CreateInterfaceFn factory);
	static void Unload();

	static IBaseClientDLL* GetClientDLL() { return pClientDLL; }

	// #include <toolframework/iclientenginetools.h>
	static IClientEngineTools* GetClientEngineTools() { return pClientEngineTools; }

	// #include <client/cliententitylist.h>
	static IClientEntityList* GetClientEntityList() { return pClientEntityList; }

	// #include <cdll_int.h>
	static IVEngineClient* GetEngineClient() { return pEngineClient; }

	// #include <toolframework/ienginetool.h>
	static IEngineTool* GetEngineTool() { return pEngineTool; }
	static IGameEventManager2* GetGameEventManager() { return pGameEventManager; }

	// #include <iprediction.h>
	static IPrediction* GetPrediction() { return pPrediction; }
	static IVModelInfoClient* GetModelInfoClient() { return pModelInfoClient; }

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

	// #include <engine/ivdebugoverlay.h>
	static IVDebugOverlay* GetDebugOverlay() { return s_DebugOverlay; }

	// #include <engine/IEngineTrace.h>
	static IEngineTrace* GetEngineTrace() { return s_EngineTrace; }

	// #include <ispatialpartition.h>
	static ISpatialPartition* GetSpatialPartition() { return s_SpatialPartition; }

private:
	static IBaseClientDLL *pClientDLL;
	static IClientEngineTools *pClientEngineTools;
	static IClientEntityList *pClientEntityList;
	static IVEngineClient *pEngineClient;
	static IEngineTool *pEngineTool;
	static IGameEventManager2 *pGameEventManager;
	static IVModelInfoClient *pModelInfoClient;
	static IPrediction *pPrediction;
	static IVRenderView *pRenderView;
	static CSteamAPIContext *pSteamAPIContext;
	static IFileSystem* s_FileSystem;
	static IVDebugOverlay* s_DebugOverlay;
	static IEngineTrace* s_EngineTrace;
	static ISpatialPartition* s_SpatialPartition;

	static bool steamLibrariesAvailable;
	static bool vguiLibrariesAvailable;

	static IClientMode*** s_ClientMode;
	static C_HLTVCamera** s_HLTVCamera;

	Interfaces() { }
	~Interfaces() { }
};