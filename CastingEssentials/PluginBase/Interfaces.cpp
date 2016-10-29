#include "Interfaces.h"
#include "Funcs.h"
#include "Exceptions.h"

#include <cdll_int.h>
#include <engine/ivmodelinfo.h>
#include <icliententitylist.h>
#include <igameevents.h>
#include <iprediction.h>
#include <ivrenderview.h>
#include <tier1/tier1.h>
#include <tier2/tier2.h>
#include <tier3/tier3.h>
#include <toolframework/iclientenginetools.h>
#include <toolframework/ienginetool.h>
#include <steam/steam_api.h>
//#include <vgui_controls/Controls.h>
#include <entitylist_base.h>

IBaseClientDLL *Interfaces::pClientDLL = nullptr;
IClientEngineTools *Interfaces::pClientEngineTools = nullptr;
IClientEntityList *Interfaces::pClientEntityList = nullptr;
IVEngineClient *Interfaces::pEngineClient = nullptr;
IEngineTool *Interfaces::pEngineTool = nullptr;
IGameEventManager2 *Interfaces::pGameEventManager = nullptr;
IPrediction *Interfaces::pPrediction = nullptr;
IVModelInfoClient *Interfaces::pModelInfoClient = nullptr;
IVRenderView *Interfaces::pRenderView = nullptr;
CSteamAPIContext *Interfaces::pSteamAPIContext = nullptr;

bool Interfaces::steamLibrariesAvailable = false;
bool Interfaces::vguiLibrariesAvailable = false;

IClientMode*** Interfaces::s_ClientMode = nullptr;
C_HLTVCamera** Interfaces::s_HLTVCamera = nullptr;

CBaseEntityList *g_pEntityList;

void Interfaces::Load(CreateInterfaceFn factory)
{
	ConnectTier1Libraries(&factory, 1);
	ConnectTier2Libraries(&factory, 1);
	ConnectTier3Libraries(&factory, 1);

	//vguiLibrariesAvailable = vgui::VGui_InitInterfacesList("CastingEssentials", &factory, 1);

	pClientEngineTools = (IClientEngineTools *)factory(VCLIENTENGINETOOLS_INTERFACE_VERSION, nullptr);
	pEngineClient = (IVEngineClient *)factory(VENGINE_CLIENT_INTERFACE_VERSION, nullptr);
	pEngineTool = (IEngineTool *)factory(VENGINETOOL_INTERFACE_VERSION, nullptr);
	pGameEventManager = (IGameEventManager2 *)factory(INTERFACEVERSION_GAMEEVENTSMANAGER2, nullptr);
	pModelInfoClient = (IVModelInfoClient *)factory(VMODELINFO_CLIENT_INTERFACE_VERSION, nullptr);
	pRenderView = (IVRenderView *)factory(VENGINE_RENDERVIEW_INTERFACE_VERSION, nullptr);

	CreateInterfaceFn gameClientFactory;
	pEngineTool->GetClientFactory(gameClientFactory);

	pClientDLL = (IBaseClientDLL*)gameClientFactory(CLIENT_DLL_INTERFACE_VERSION, nullptr);
	pClientEntityList = (IClientEntityList*)gameClientFactory(VCLIENTENTITYLIST_INTERFACE_VERSION, nullptr);
	pPrediction = (IPrediction *)gameClientFactory(VCLIENT_PREDICTION_INTERFACE_VERSION, nullptr);

	pSteamAPIContext = new CSteamAPIContext();
	steamLibrariesAvailable = SteamAPI_InitSafe() && pSteamAPIContext->Init();

	g_pEntityList = dynamic_cast<CBaseEntityList *>(Interfaces::pClientEntityList);
}

void Interfaces::Unload()
{
	vguiLibrariesAvailable = false;
	steamLibrariesAvailable = false;

	s_ClientMode = nullptr;
	s_HLTVCamera = nullptr;

	pClientEngineTools = nullptr;
	pEngineClient = nullptr;
	pEngineTool = nullptr;
	pGameEventManager = nullptr;
	pModelInfoClient = nullptr;
	pRenderView = nullptr;

	pClientDLL = nullptr;
	pClientEntityList = nullptr;
	pPrediction = nullptr;

	pSteamAPIContext = nullptr;

	DisconnectTier3Libraries();
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();
}

IClientMode* Interfaces::GetClientMode()
{
	if (!s_ClientMode)
	{
		constexpr const char* SIG = "\xC7\x05\x00\x00\x00\x00\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x68\x00\x00\x00\x00\x8B\xC8";
		constexpr const char* MASK = "xx????????x????x????xx";
		constexpr auto OFFSET = 2;

		s_ClientMode = (IClientMode***)((char*)SignatureScan("client", SIG, MASK) + OFFSET);

		if (!s_ClientMode)
			throw bad_pointer("IClientMode");
	}

	IClientMode* deref = **s_ClientMode;
	if (!deref)
		throw bad_pointer("IClientMode");

	return deref;
}

C_HLTVCamera* Interfaces::GetHLTVCamera()
{
	if (!s_HLTVCamera)
	{
		constexpr const char* SIG = "\xB9\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x68\x00\x00\x00\x00\xC7\x05\x00\x00\x00\x00\x00\x00\x00\x00\xC6\x05\x00\x00\x00\x00\x00";
		constexpr const char* MASK = "x????x????x????xx????xxxxxx????x";
		constexpr auto OFFSET = 1;

		s_HLTVCamera = (C_HLTVCamera**)((char*)SignatureScan("client", SIG, MASK) + OFFSET);

		if (!s_HLTVCamera)
			throw bad_pointer("C_HLTVCamera");
	}

	C_HLTVCamera* deref = *s_HLTVCamera;
	if (!deref)
		throw bad_pointer("C_HLTVCamera");

	return deref;
}