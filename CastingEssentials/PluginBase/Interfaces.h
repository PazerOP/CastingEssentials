#pragma once

#include "interface.h"
#include <memory>

class C_HLTVCamera;
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

class Interfaces final
{
public:
	static void Load(CreateInterfaceFn factory);
	static void Unload();

	static IBaseClientDLL* GetClientDLL() { return pClientDLL; }
	static IClientEngineTools* GetClientEngineTools() { return pClientEngineTools; }
	static IClientEntityList* GetClientEntityList() { return pClientEntityList; }

	// #include <cdll_int.h>
	static IVEngineClient* GetEngineClient() { return pEngineClient; }
	static IEngineTool* GetEngineTool() { return pEngineTool; }
	static IGameEventManager2* GetGameEventManager() { return pGameEventManager; }
	static IPrediction* GetPrediction() { return pPrediction; }
	static IVModelInfoClient* GetModelInfoClient() { return pModelInfoClient; }
	static IVRenderView* GetRenderView() { return pRenderView; }

	// #include <steam/steam_api.h>
	static CSteamAPIContext* GetSteamAPIContext() { return pSteamAPIContext; }

	static bool AreSteamLibrariesAvailable() { return steamLibrariesAvailable; }
	static bool AreVguiLibrariesAvailable() { return vguiLibrariesAvailable; }

	// #include <client/iclientmode.h>
	static IClientMode *GetClientMode();
	static C_HLTVCamera *GetHLTVCamera();

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

	static bool steamLibrariesAvailable;
	static bool vguiLibrariesAvailable;

	static IClientMode*** s_ClientMode;
	static C_HLTVCamera** s_HLTVCamera;

	Interfaces() { }
	~Interfaces() { }
};