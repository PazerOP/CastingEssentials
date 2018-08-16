#include "Interfaces.h"
#include "Hooking/ModuleSegments.h"
#include "Misc/HLTVCameraHack.h"
#include "PluginBase/HookManager.h"
#include "Exceptions.h"

#include <cdll_int.h>
#include <engine/IStaticPropMgr.h>
#include <engine/ivmodelinfo.h>
#include <game/client/iclientrendertargets.h>
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
#include <vgui_controls/Controls.h>
#include <entitylist_base.h>
#include <filesystem.h>
#include <engine/ivdebugoverlay.h>
#include <engine/IEngineTrace.h>
#include <client/cliententitylist.h>
#include <shaderapi/ishaderapi.h>

#include <Windows.h>

IBaseClientDLL *Interfaces::pClientDLL = nullptr;
IClientEngineTools *Interfaces::pClientEngineTools = nullptr;
IClientEntityList *Interfaces::pClientEntityList = nullptr;
IStaticPropMgrClient* Interfaces::s_StaticPropMgr;
IVEngineClient *Interfaces::pEngineClient = nullptr;
IEngineTool *Interfaces::pEngineTool = nullptr;
IGameEventManager2 *Interfaces::pGameEventManager = nullptr;
IPrediction *Interfaces::pPrediction = nullptr;
IVModelInfoClient *Interfaces::pModelInfoClient = nullptr;
IVRenderView *Interfaces::pRenderView = nullptr;
IMaterialSystem* Interfaces::pMaterialSystem = nullptr;
IShaderAPI* Interfaces::s_ShaderAPI = nullptr;
CSteamAPIContext *Interfaces::pSteamAPIContext = nullptr;
IFileSystem* Interfaces::s_FileSystem = nullptr;
IVDebugOverlay* Interfaces::s_DebugOverlay = nullptr;
IEngineTrace* Interfaces::s_EngineTrace = nullptr;
ISpatialPartition* Interfaces::s_SpatialPartition = nullptr;
IClientLeafSystem* Interfaces::s_ClientLeafSystem = nullptr;
IClientRenderTargets* Interfaces::s_ClientRenderTargets = nullptr;

bool Interfaces::steamLibrariesAvailable = false;
bool Interfaces::vguiLibrariesAvailable = false;

IClientMode* Interfaces::s_ClientMode = nullptr;
C_HLTVCamera** Interfaces::s_HLTVCamera = nullptr;
C_HLTVCamera* HLTVCamera() { return Interfaces::GetHLTVCamera(); }

CClientEntityList* cl_entitylist;
CBaseEntityList *g_pEntityList;
IGameEventManager2* gameeventmanager;
IVDebugOverlay* debugoverlay;
IEngineTrace* enginetrace;
IVEngineClient* engine;
IVRenderView* render;
IClientLeafSystem* g_pClientLeafSystem;
IClientRenderTargets* g_pClientRenderTargets;

void Interfaces::Load(CreateInterfaceFn factory)
{
	ConnectTier1Libraries(&factory, 1);
	ConnectTier2Libraries(&factory, 1);
	ConnectTier3Libraries(&factory, 1);

	if (!factory)
		Error(__FUNCTION__ ": factory was null");

	vguiLibrariesAvailable = vgui::VGui_InitInterfacesList("CastingEssentials", &factory, 1);

	pClientEngineTools = (IClientEngineTools *)factory(VCLIENTENGINETOOLS_INTERFACE_VERSION, nullptr);
	pEngineClient = (IVEngineClient *)factory(VENGINE_CLIENT_INTERFACE_VERSION, nullptr);
	pEngineTool = (IEngineTool *)factory(VENGINETOOL_INTERFACE_VERSION, nullptr);
	pGameEventManager = (IGameEventManager2 *)factory(INTERFACEVERSION_GAMEEVENTSMANAGER2, nullptr);
	pModelInfoClient = (IVModelInfoClient *)factory(VMODELINFO_CLIENT_INTERFACE_VERSION, nullptr);
	pRenderView = (IVRenderView *)factory(VENGINE_RENDERVIEW_INTERFACE_VERSION, nullptr);
	pMaterialSystem = (IMaterialSystem*)factory(MATERIAL_SYSTEM_INTERFACE_VERSION, nullptr);
	s_ShaderAPI = (IShaderAPI*)factory(SHADERAPI_INTERFACE_VERSION, nullptr);
	s_FileSystem = (IFileSystem*)factory(FILESYSTEM_INTERFACE_VERSION, nullptr);
	s_DebugOverlay = (IVDebugOverlay*)factory(VDEBUG_OVERLAY_INTERFACE_VERSION, nullptr);
	s_EngineTrace = (IEngineTrace*)factory(INTERFACEVERSION_ENGINETRACE_CLIENT, nullptr);
	s_SpatialPartition = (ISpatialPartition*)factory(INTERFACEVERSION_SPATIALPARTITION, nullptr);
	s_ClientRenderTargets = (IClientRenderTargets*)factory(CLIENTRENDERTARGETS_INTERFACE_VERSION, nullptr);
	s_StaticPropMgr = (IStaticPropMgrClient*)factory(INTERFACEVERSION_STATICPROPMGR_CLIENT, nullptr);

	CreateInterfaceFn gameClientFactory;
	pEngineTool->GetClientFactory(gameClientFactory);

	if (!gameClientFactory)
		Error(__FUNCTION__ ": gameClientFactory was null");

	pClientDLL = (IBaseClientDLL*)gameClientFactory(CLIENT_DLL_INTERFACE_VERSION, nullptr);
	pClientEntityList = (IClientEntityList*)gameClientFactory(VCLIENTENTITYLIST_INTERFACE_VERSION, nullptr);
	pPrediction = (IPrediction *)gameClientFactory(VCLIENT_PREDICTION_INTERFACE_VERSION, nullptr);
	s_ClientLeafSystem = (IClientLeafSystem*)gameClientFactory(CLIENTLEAFSYSTEM_INTERFACE_VERSION, nullptr);
	s_ClientRenderTargets = (IClientRenderTargets*)gameClientFactory(CLIENTRENDERTARGETS_INTERFACE_VERSION, nullptr);

	pSteamAPIContext = new CSteamAPIContext();
	steamLibrariesAvailable = SteamAPI_InitSafe() && pSteamAPIContext->Init();

	// Built in declarations
	{
		cl_entitylist = dynamic_cast<CClientEntityList*>(Interfaces::GetClientEntityList());
		g_pEntityList = dynamic_cast<CBaseEntityList*>(Interfaces::GetClientEntityList());
		debugoverlay = Interfaces::GetDebugOverlay();
		enginetrace = Interfaces::GetEngineTrace();
		engine = Interfaces::GetEngineClient();
		render = Interfaces::GetRenderView();
		materials = Interfaces::GetMaterialSystem();
		g_pClientLeafSystem = Interfaces::GetClientLeafSystem();
		gameeventmanager = Interfaces::GetGameEventManager();
	}
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

	// Built in declarations
	cl_entitylist = nullptr;
	g_pEntityList = nullptr;
	debugoverlay = nullptr;
	enginetrace = nullptr;
	engine = nullptr;
	engine = nullptr;
	render = nullptr;
	materials = nullptr;
	g_pStudioRender = nullptr;
	g_pClientLeafSystem = nullptr;
	gameeventmanager = nullptr;

	DisconnectTier3Libraries();
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();
}

IClientMode* Interfaces::GetClientMode()
{
	if (!s_ClientMode)
	{
		constexpr const char* SIG = "\xA1????\xA8\x01\x75\x1F\x83\xC8\x01\xB9????\xA3????\xE8????\x68????\xE8????\x83\xC4\x04\xB8????\xC3\xCC\xCC\x8B\x0D????";
		constexpr const char* MASK = "x????xxxxxxxx????x????x????x????x????xxxx????xxxxx????";
		constexpr auto OFFSET = 0;

		typedef IClientMode* (*GetClientModeFn)();

		auto fn = (GetClientModeFn)((char*)SignatureScan("client", SIG, MASK) + OFFSET);

		if (!fn)
			throw bad_pointer("IClientMode");

		IClientMode* icm = fn();
		if (!icm)
			throw bad_pointer("IClientMode");

		s_ClientMode = icm;
	}

	return s_ClientMode;
}

HLTVCameraOverride* Interfaces::GetHLTVCamera()
{
	if (!s_HLTVCamera)
	{
		constexpr const char* SIG = "\xB9\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x68\x00\x00\x00\x00\xC7\x05\x00\x00\x00\x00\x00\x00\x00\x00\xC6\x05\x00\x00\x00\x00\x00";
		constexpr const char* MASK = "x????x????x????xx????xxxxxx????x";
		constexpr auto OFFSET = 1;

		s_HLTVCamera = (C_HLTVCamera**)((char*)SignatureScan("client", SIG, MASK) + OFFSET);

		if (s_HLTVCamera == (C_HLTVCamera**)OFFSET)
			throw bad_pointer("C_HLTVCamera");
	}

	C_HLTVCamera* deref = *s_HLTVCamera;
	if (!deref)
		throw bad_pointer("C_HLTVCamera");

	return (HLTVCameraOverride*)deref;
}

C_BasePlayer*& Interfaces::GetLocalPlayer()
{
	static C_BasePlayer** s_LocalPlayer = nullptr;
	if (!s_LocalPlayer)
	{
		auto localPlayerFn = HookManager::GetRawFunc<HookFunc::C_BasePlayer_GetLocalPlayer>();

		auto location = *(intptr_t*)((std::byte*)localPlayerFn + 1);

		s_LocalPlayer = (C_BasePlayer**)location;
	}

	return *s_LocalPlayer;
}

cmdalias_t** Interfaces::GetCmdAliases()
{
	static cmdalias_t** s_CmdAliases = nullptr;
	if (!s_CmdAliases)
	{
		auto cmdShutdownFn = HookManager::GetRawFunc<HookFunc::Global_Cmd_Shutdown>();

		auto location = *(intptr_t*)((std::byte*)cmdShutdownFn + 1);

		s_CmdAliases = (cmdalias_t**)location;
	}

	return s_CmdAliases;
}

C_GameRules* Interfaces::GetGameRules()
{
	static C_GameRules** s_GameRules = nullptr;
	if (!s_GameRules)
	{
		constexpr const char* SIG = "\x8B\x0D????\x8B\x01\x8B\x80????\xFF\xD0\x84\xC0\x74\x1E";
		constexpr const char* MASK = "xx????xxxx????xxxxxx";
		constexpr auto OFFSET = 2;

		if (auto gr = (std::byte*)SignatureScan("client", SIG, MASK))
			s_GameRules = *(C_GameRules***)(gr + OFFSET);
		else
			throw bad_pointer("C_GameRules");
	}

	return *s_GameRules;
}

CUtlVector<IGameSystem*>* Interfaces::GetGameSystems()
{
	static CUtlVector<IGameSystem*>* s_GameSystems = nullptr;
	if (!s_GameSystems)
	{
		constexpr const char* SIG = "\xB9????\x89\x75\xFC\xE8????\x6A\x00";
		constexpr const char* MASK = "x????xxxx????xx";
		constexpr auto OFFSET = 1;

		if (auto p = (std::byte*)SignatureScan("client", SIG, MASK))
			s_GameSystems = *(CUtlVector<IGameSystem*>**)(p + OFFSET);
		else
			throw bad_pointer("CUtlVector<IGameSystem*>");
	}

	return s_GameSystems;
}

CUtlVector<IGameSystemPerFrame*>* Interfaces::GetGameSystemsPerFrame()
{
	static CUtlVector<IGameSystemPerFrame*>* s_GameSystems = nullptr;
	if (!s_GameSystems)
	{
		constexpr const char* SIG = "\xB9????\xE8????\x8D\x45\xFC\xC7\x06";
		constexpr const char* MASK = "x????x????xxxxx";
		constexpr auto OFFSET = 1;

		if (auto p = (std::byte*)SignatureScan("client", SIG, MASK))
			s_GameSystems = *(CUtlVector<IGameSystemPerFrame*>**)(p + OFFSET);
		else
			throw bad_pointer("CUtlVector<IGameSystemPerFrame*>");
	}

	return s_GameSystems;
}

ITextureManager* Interfaces::GetTextureManager()
{
	static ITextureManager* s_TextureManager = []() -> ITextureManager*
	{
		// First find the CTextureManager constructor
		void* constructor = SignatureScan("MaterialSystem", "\x56\x8B\xF1\x57\xC7\x06????\xC7\x46", "xxxxxx????xx");
		if (!constructor)
			return nullptr;

		constexpr const char* SIG = "\xB9????\xE8????\x68????\xE8????\x59\xC3";
		constexpr const char* MASK = "x????x????x????x????xx";

		// Then find the static initialization function for the s_TextureManager variable
		auto initFn = SignatureScanMultiple("MaterialSystem", SIG, MASK,
			[&](std::byte* found)
		{
			int offset2 = *(int*)(found + 6);
			Assert(true);

			return (found + offset2 + 10) == constructor;
		});

		if (!initFn)
			return nullptr;

		// First instruction of the init function is mov ecx, offset s_TextureManager
		int offset = *(int*)(initFn + 1); // Get the offset of the B9 (mov relative w/ 32-bit address) instruction
		auto mgr = (ITextureManager*)offset;
		Assert(mgr);

		return mgr;
	}();

	return s_TextureManager;
}
