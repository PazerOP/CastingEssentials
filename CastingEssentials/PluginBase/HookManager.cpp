#include "HookManager.h"
#include "Controls/StubPanel.h"
#include "Misc/HLTVCameraHack.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Exceptions.h"

#include <PolyHook.hpp>

#include <Windows.h>
#include <Psapi.h>
#include <icvar.h>
#include <cdll_int.h>
#include <client/hltvcamera.h>
#include <toolframework/iclientenginetools.h>
#include <iprediction.h>

static std::unique_ptr<HookManager> s_HookManager;
HookManager* GetHooks() { Assert(s_HookManager); return s_HookManager.get(); }

void* HookManager::s_RawFunctions[(int)HookFunc::Count];

bool HookManager::Load()
{
	s_HookManager.reset(new HookManager());
	return true;
}
bool HookManager::Unload()
{
	s_HookManager.reset();
	return true;
}

class HookManager::Panel final : vgui::StubPanel
{
public:
	Panel()
	{
		if (!Interfaces::GetEngineClient())
			Error("[CastingEssentials] Interfaces::GetEngineClient() returned nullptr.");

		m_InGame = false;
	}
	void OnTick() override
	{
		if (!m_InGame && Interfaces::GetEngineClient()->IsInGame())
		{
			m_InGame = true;
			GetHooks()->IngameStateChanged(m_InGame);
		}
		else if (m_InGame && !Interfaces::GetEngineClient()->IsInGame())
		{
			m_InGame = false;
			GetHooks()->IngameStateChanged(m_InGame);
		}
	}
private:
	bool m_InGame;
};

static bool DataCompare(const BYTE* pData, const BYTE* bSig, const char* szMask)
{
	for (; *szMask; ++szMask, ++pData, ++bSig)
	{
		if (*szMask == 'x' && *pData != *bSig)
			return false;
	}

	return (*szMask) == NULL;
}

static void* FindPattern(DWORD dwAddress, DWORD dwSize, BYTE* pbSig, const char* szMask)
{
	for (DWORD i = NULL; i < dwSize; i++)
	{
		if (DataCompare((BYTE*)(dwAddress + i), pbSig, szMask))
			return (void*)(dwAddress + i);
	}

	return nullptr;
}

void* SignatureScan(const char* moduleName, const char* signature, const char* mask)
{
	MODULEINFO clientModInfo;
	const HMODULE clientModule = GetModuleHandle((std::string(moduleName) + ".dll").c_str());
	GetModuleInformation(GetCurrentProcess(), clientModule, &clientModInfo, sizeof(MODULEINFO));

	return FindPattern((DWORD)clientModInfo.lpBaseOfDll, clientModInfo.SizeOfImage, (PBYTE)signature, mask);
}

#if 0
HookManager::Raw_RenderBox HookManager::GetRawFunc_RenderBox()
{
	static Raw_RenderBox fn = nullptr;
	if (!fn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x51\x8B\x45\x18\x8B\xC8";
		constexpr const char* MASK = "xxxxxxxxx";

		fn = (Raw_RenderBox)SignatureScan("engine", SIG, MASK);
		if (!fn)
			throw bad_pointer("RenderBox");
	}

	return fn;
}

HookManager::Raw_RenderBox_1 HookManager::GetRawFunc_RenderBox_1()
{
	static Raw_RenderBox_1 fn = nullptr;
	if (!fn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x81\xEC????\x56\xE8????\x8B\x0D????\x8B\x01\xFF\x90????\x8B\xF0\x89\x75\xDC";
		constexpr const char* MASK = "xxxxx????xx????xx????xxxx????xxxxx";

		fn = (Raw_RenderBox_1)SignatureScan("engine", SIG, MASK);
		if (!fn)
			throw bad_pointer("RenderBox_Impl");
	}

	return fn;
}

HookManager::Raw_RenderWireframeBox HookManager::GetRawFunc_RenderWireframeBox()
{
	static Raw_RenderWireframeBox fn = nullptr;
	if (!fn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x81\xEC????\x56\xE8????\x8B\x0D????\x8B\x01\xFF\x90????\x8B\xF0\x89\x75\xF8";
		constexpr const char* MASK = "xxxxx????xx????xx????xxxx????xxxxx";

		fn = (Raw_RenderWireframeBox)SignatureScan("engine", SIG, MASK);
		if (!fn)
			throw bad_pointer("RenderWireframeBox");
	}

	return fn;
}

HookManager::Raw_RenderLine HookManager::GetRawFunc_RenderLine()
{
	static Raw_RenderLine fn = nullptr;
	if (!fn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x81\xEC????\x56\xE8????\x8B\x0D????\x8B\x01\xFF\x90????\x8B\xF0\x85\xF6";
		constexpr const char* MASK = "xxxxx????xx????xx????xxxx????xxxx";

		fn = (Raw_RenderLine)SignatureScan("engine", SIG, MASK);
		if (!fn)
			throw bad_pointer("RenderLine");
	}

	return fn;
}

HookManager::Raw_RenderTriangle HookManager::GetRawFunc_RenderTriangle()
{
	static Raw_RenderTriangle fn = nullptr;
	if (!fn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x51\x8B\x45\x14\x8B\xC8";
		constexpr const char* MASK = "xxxxxxxxx";

		fn = (Raw_RenderTriangle)SignatureScan("engine", SIG, MASK);
		if (!fn)
			throw bad_pointer("RenderTriangle");
	}

	return fn;
}
#endif

void HookManager::FindFunc_C_BasePlayer_GetLocalPlayer()
{
	// We know that C_BasePlayer::GetLocalPlayerIndex() immediately calls C_BasePlayer::GetLocalPlayer().
	// Since GetLocalPlayer just returns a global variable, it's not reliable to find it with a signature.
	// Instead, we find GetLocalPlayer indirectly through GetLocalPlayerIndex.
	auto localPlayerIndexFn = GetRawFunc<HookFunc::Global_GetLocalPlayerIndex>();

	// We only handle relative offsets, this needs an update if GetLocalPlayerIndex signature ever changes
	Assert(*(uint8_t*)localPlayerIndexFn == 0xE8);
	auto offset = *(int*)((std::byte*)localPlayerIndexFn + 1);

	s_RawFunctions[(int)HookFunc::C_BasePlayer_GetLocalPlayer] = ((std::byte*)localPlayerIndexFn + 5 + offset);
}

void HookManager::InitRawFunctionsList()
{
	FindFunc<HookFunc::Global_CreateEntityByName>("\x55\x8B\xEC\xE8????\xFF\x75\x08\x8B\xC8\x8B\x10\xFF??\x85\xC0\x75\x13\xFF\x75\x08\x68????\xFF?????\x83\xC4\x08\x33\xC0\x5D\xC3", "xxxx????xxxxxxxx??xxxxxxxx????x?????xxxxxxx");
	FindFunc<HookFunc::Global_CreateTFGlowObject>("\x55\x8B\xEC\x57\x68????\xE8????\x8B\xF8\x83\xC4\x04\x85\xFF\x74\x5F\x56\x8B\xCF\xE8????\xFF\x75\x0C\xC7\x07", "xxxxx????x????xxxxxxxxxxxxx????xxxxx");
	FindFunc<HookFunc::Global_DrawOpaqueRenderable>("\x55\x8B\xEC\x83\xEC\x20\x8B\x0D????\x53\x56\x33\xF6", "xxxxxxxx????xxxx");
	FindFunc<HookFunc::Global_DrawTranslucentRenderable>("\x55\x8B\xEC\x83\xEC\x0C\x53\x8B\x5D\x08\x8B\xCB\x8B\x03\xFF\x50\x34", "xxxxxxxxxxxxxxxxx");
	FindFunc<HookFunc::Global_GetLocalPlayerIndex>("\xE8????\x85\xC0\x74\x08\x8D\x48\x08\x8B\x01\xFF\x60\x24\x33\xC0\xC3", "x????xxxxxxxxxxxxxxx");
	FindFunc<HookFunc::Global_GetVectorInScreenSpace>("\x55\x8B\xEC\x8B\x45\x1C\x83\xEC\x10", "xxxxxxxxx");
	FindFunc<HookFunc::Global_UserInfoChangedCallback>("\x55\x8B\xEC\x53\x8B\x5D\x18\x85\xDB\x0F\x84????\x56", "xxxxxxxxxxx????x", "engine");
	FindFunc<HookFunc::Global_UTILComputeEntityFade>("\x55\x8B\xEC\x83\xEC\x40\x80\x3D", "xxxxxxxx");
	FindFunc<HookFunc::Global_UTIL_TraceLine>("\x53\x8B\xDC\x83\xEC\x08\x83\xE4\xF0\x83\xC4\x04\x55\x8B\x6B\x04\x89\x6C\x24\x04\x8B\xEC\x83\xEC\x6C\x8D\x4D\xA0\x56\xFF\x73\x0C", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

	FindFunc<HookFunc::C_BaseAnimating_ComputeHitboxSurroundingBox>("\x55\x8B\xEC\x81\xEC????\x56\x8B\xF1\x57\x80\xBE?????\x0F\x85????\x83\xBE?????\x75\x05\xE8????\x8B\xBE????\x85\xFF\x0F\x84????\x8B\x86????\x8B\x17\x53", "xxxxx????xxxxxx?????xx????xx?????xxx????xx????xxxx????xx????xxx");
	FindFunc<HookFunc::C_BaseAnimating_DrawModel>("\x55\x8B\xEC\x83\xEC\x20\x8B\x15????\x53\x33\xDB\x56", "xxxxxxxx????xxxx");
	FindFunc<HookFunc::C_BaseAnimating_GetBoneCache>("\x55\x8B\xEC\x83\xEC\x10\x56\x8B\xF1\x57\xFF\xB6", "xxxxxxxxxxxx");
	FindFunc<HookFunc::C_BaseAnimating_GetBonePosition>("\x55\x8B\xEC\x83\xEC\x30\x56\x6A\x00", "xxxxxxxxx");
	FindFunc<HookFunc::C_BaseAnimating_InternalDrawModel>("\x55\x8B\xEC\x81\xEC????\x53\x56\x57\x8B\xF9\xC6\x45\xFF\x00\x8B\x87", "xxxxx????xxxxxxxxxxx");
	FindFunc<HookFunc::C_BaseAnimating_LockStudioHdr>("\x55\x8B\xEC\x83\xEC\x20\x56\x57\x6A\x01\x68????\x8B\xF1", "xxxxxxxxxxx????xx");
	FindFunc<HookFunc::C_BaseAnimating_LookupBone>("\x55\x8B\xEC\x56\x8B\xF1\x80\xBE\x00\x00\x00\x00\x00\x74\x13\xFF\x75\x08\x33\xC0\x50\xE8\x00\x00\x00\x00\x83\xC4\x08\x5E\x5D\xC2\x04\x00\x83\xBE\x00\x00\x00\x00\x00\x75\x05\xE8\x00\x00\x00\x00\xFF\x75\x08\x8B\x86\x00\x00\x00\x00\x50\xE8\x00\x00\x00\x00\x83\xC4\x08\x5E\x5D\xC2\x04\x00", "xxxxxxxx?????xxxxxxxxx????xxxxxxxxxx?????xxx????xxxxx????xx????xxxxxxxx");

	FindFunc<HookFunc::C_BaseEntity_Init>("\x55\x8B\xEC\x8B\x45\x08\x56\xFF\x75\x0C\x8B\xF1\x8D\x4E\x04", "xxxxxxxxxxxxxxx");
	FindFunc<HookFunc::C_BaseEntity_CalcAbsolutePosition>("\x55\x8B\xEC\x81\xEC????\x80\x3D?????\x53\x8B\xD9\x0F\x84", "xxxxx????xx?????xxxxx");

	FindFunc<HookFunc::C_BasePlayer_GetDefaultFOV>("\x57\x8B\xF9\x8B\x07\xFF\x90????\x83\xF8\x04", "xxxxxxx????xxx");
	FindFunc<HookFunc::C_BasePlayer_GetFOV>("\x55\x8B\xEC\x83\xEC\x10\x56\x8B\xF1\x8B\x0D????\x8B\x01", "xxxxxxxxxxx????xx");
	FindFunc<HookFunc::C_BasePlayer_ShouldDrawLocalPlayer>("\x8B\x0D????\x85\xC9\x74\x3B\x8B\x01", "xx????xxxxxx");

	FindFunc<HookFunc::C_HLTVCamera_SetCameraAngle>("\x55\x8B\xEC\x8B\x45\x08\x56\x8B\xF1\x8D\x56\x00\xD9\x00\xD9\x1A\xD9\x40\x00\xD9\x5A\x00\xD9\x40\x00\x52", "xxxxxxxxxxx?xxxxxx?xx?xx?x");
	FindFunc<HookFunc::C_HLTVCamera_SetMode>("\x55\x8B\xEC\x8B\x45\x08\x53\x56\x8B\xF1\x8B\x5E\x00", "xxxxxxxxxxxx?");
	FindFunc<HookFunc::C_HLTVCamera_SetPrimaryTarget>("\x55\x8B\xEC\x8B\x45\x08\x83\xEC\x00\x53\x56\x8B\xF1", "xxxxxxxx?xxxx");

	FindFunc<HookFunc::C_TFPlayer_DrawModel>("\x55\x8B\xEC\x51\x57\x8B\xF9\x80\x7F\x54\x17", "xxxxxxxxxxx");

	FindFunc<HookFunc::CAccountPanel_OnAccountValueChanged>("\x55\x8B\xEC\x51\x53\x8B\x5D\x0C\x56\x8B\xF1\x53", "xxxxxxxxxxxx");
	FindFunc<HookFunc::CAccountPanel_Paint>("\x55\x8B\xEC\x83\xEC\x74\x56\x8B\xC1", "xxxxxxxxx");
	FindFunc<HookFunc::CDamageAccountPanel_DisplayDamageFeedback>("\x55\x8B\xEC\x81\xEC????\x83\x7D\x10\x00\x53\x8B\xD9\x0F\x8E", "xxxxx????xxxxxxxxx");
	FindFunc<HookFunc::CDamageAccountPanel_ShouldDraw>("\x56\x8B\xF1\xE8????\x8B\xC8\x85\xC9\x74\x0E", "xxxx????xxxxxx");

	FindFunc<HookFunc::CGlowObjectManager_ApplyEntityGlowEffects>("\x55\x8B\xEC\x83\xEC\x3C\x53\x56\x57\x8B\xD9\x8B\x0D", "xxxxxxxxxxxxx");

	FindFunc<HookFunc::CHudBaseDeathNotice_GetIcon>("\x55\x8B\xEC\x81\xEC????\x83\x7D\x0C\x00\x56", "xxxxx????xxxxx");

	FindFunc<HookFunc::IGameSystem_Add>("\x55\x8B\xEC\x51\x8B\x15????\x8B\x0D", "xxxxxx????xx");
	FindFunc<HookFunc::IGameSystem_Remove>("\x55\x8B\xEC\x51\x56\x8B\xF1\x8D\x45\xFC\x50\xB9????\x89\x75\xFC\xC7\x06????\xE8????\x6A\x00\x68????\x68????\x6A\x00\x56\xE8????\x83\xC4\x14\x85\xC0\x74\x11\x8D\x45\xFC\x89\x75\xFC\x50\xB9????\xE8????\x5E", "xxxxxxxxxxxx????xxxxx????x????xxx????x????xxxx????xxxxxxxxxxxxxxx????x????x");

	FindFunc<HookFunc::vgui_EditablePanel_GetDialogVariables>("\x56\x8B\xF1\x8B\x86????\x85\xC0\x75\x2A", "xxxxx????xxxx");
	FindFunc<HookFunc::vgui_ImagePanel_SetImage>("\x55\x8B\xEC\x53\x57\x8B\x7D\x08\x8B\xD9\x85\xFF\x74\x18", "xxxxxxxxxxxxxx");
	FindFunc<HookFunc::vgui_ProgressBar_ApplySettings>("\x55\x8B\xEC\xD9\xEE\x53", "xxxxxx");

	FindFunc<HookFunc::CModelLoader_Map_LoadModel>("\x55\x8B\xEC\x81\xEC????\xFF\x05", "xxxxx????xx", "engine");
	FindFunc<HookFunc::Global_Map_CheckForHDR>("\x55\x8B\xEC\x51\xFF\x75\x0C", "xxxxxxx", "engine");
	FindFunc<HookFunc::Global_EnableHDR>("\x55\x8B\xEC\x8B\x0D????\x8B\x01\x8B\x80????\xFF\xD0\x8B\x55\x08", "xxxxx????xxxx????xxxxx", "engine");
	FindFunc<HookFunc::CTextureManager_LoadTexture>("\x55\x8B\xEC\x83\xEC\x0C\x53\x56\x57\xFF\x75\x0C", "xxxxxxxxxxxx", "materialsystem");

	FindFunc_C_BasePlayer_GetLocalPlayer();
}

template<HookFunc fn> void HookManager::FindFunc(const char* signature, const char* mask, const char* module, int offset)
{
	std::byte* result = (std::byte*)SignatureScan(module, signature, mask);
	if (result)
	{
		result += offset;
	}
	else
	{
		Assert(!"Failed to find function!");
		result = nullptr;
	}

	s_RawFunctions[(int)fn] = (void*)result;
}

void HookManager::IngameStateChanged(bool inGame)
{
	if (inGame)
	{
		GetHook<HookFunc::IGameEventManager2_FireEventClientSide>()->AttachHook(std::make_shared<HookFuncType<HookFunc::IGameEventManager2_FireEventClientSide>::Hook::Inner>(Interfaces::GetGameEventManager(), &IGameEventManager2::FireEventClientSide));
	}
	else
	{
		GetHook<HookFunc::IGameEventManager2_FireEventClientSide>()->DetachHook();
	}
}

template<HookFunc fn, class... Args>
void HookManager::InitHook(Args&&... args)
{
	m_Hooks[(int)fn] = std::make_unique<HookFuncType<fn>::Hook>();
	GetHook<fn>()->AttachHook(std::make_shared<HookFuncType<fn>::Hook::Inner>(args...));
}

template<HookFunc fn> void HookManager::InitGlobalHook()
{
	InitHook<fn>(GetRawFunc<fn>());
}

HookManager::HookManager()
{
	InitRawFunctionsList();

	Assert(!s_HookManager);
	m_Panel.reset(new Panel());

	InitHook<HookFunc::ICvar_ConsoleColorPrintf>(g_pCVar, &ICvar::ConsoleColorPrintf);
	InitHook<HookFunc::ICvar_ConsoleDPrintf>(g_pCVar, &ICvar::ConsoleDPrintf);
	InitHook<HookFunc::ICvar_ConsolePrintf>(g_pCVar, &ICvar::ConsolePrintf);

	InitHook<HookFunc::IClientEngineTools_InToolMode>(Interfaces::GetClientEngineTools(), &IClientEngineTools::InToolMode);
	InitHook<HookFunc::IClientEngineTools_IsThirdPersonCamera>(Interfaces::GetClientEngineTools(), &IClientEngineTools::IsThirdPersonCamera);
	InitHook<HookFunc::IClientEngineTools_SetupEngineView>(Interfaces::GetClientEngineTools(), &IClientEngineTools::SetupEngineView);

	// Just construct but don't init, gameeventmanager isn't instantiated until we load a map
	m_Hooks[(int)HookFunc::IGameEventManager2_FireEventClientSide] = std::make_unique<HookFuncType<HookFunc::IGameEventManager2_FireEventClientSide>::Hook>();

	InitHook<HookFunc::IVEngineClient_GetPlayerInfo>(Interfaces::GetEngineClient(), &IVEngineClient::GetPlayerInfo);

	InitHook<HookFunc::IPrediction_PostEntityPacketReceived>(Interfaces::GetPrediction(), &IPrediction::PostEntityPacketReceived);

	InitHook<HookFunc::IStudioRender_ForcedMaterialOverride>(g_pStudioRender, &IStudioRender::ForcedMaterialOverride);

	InitHook<HookFunc::C_HLTVCamera_SetCameraAngle>(Interfaces::GetHLTVCamera(), GetRawFunc<HookFunc::C_HLTVCamera_SetCameraAngle>());
	InitHook<HookFunc::C_HLTVCamera_SetMode>(Interfaces::GetHLTVCamera(), GetRawFunc<HookFunc::C_HLTVCamera_SetMode>());
	InitHook<HookFunc::C_HLTVCamera_SetPrimaryTarget>(Interfaces::GetHLTVCamera(), GetRawFunc<HookFunc::C_HLTVCamera_SetPrimaryTarget>());

	InitGlobalHook<HookFunc::C_BaseAnimating_DrawModel>();
	InitGlobalHook<HookFunc::C_BaseAnimating_InternalDrawModel>();
	InitGlobalHook<HookFunc::C_BasePlayer_GetDefaultFOV>();
	InitGlobalHook<HookFunc::C_TFPlayer_DrawModel>();
	InitGlobalHook<HookFunc::vgui_ProgressBar_ApplySettings>();

	InitGlobalHook<HookFunc::C_BaseEntity_Init>();

	InitGlobalHook<HookFunc::CGlowObjectManager_ApplyEntityGlowEffects>();

	InitGlobalHook<HookFunc::CAccountPanel_OnAccountValueChanged>();
	InitGlobalHook<HookFunc::CAccountPanel_Paint>();
	InitGlobalHook<HookFunc::CDamageAccountPanel_DisplayDamageFeedback>();
	InitGlobalHook<HookFunc::CDamageAccountPanel_ShouldDraw>();

	InitGlobalHook<HookFunc::Global_CreateEntityByName>();
	InitGlobalHook<HookFunc::Global_DrawOpaqueRenderable>();
	InitGlobalHook<HookFunc::Global_DrawTranslucentRenderable>();
	InitGlobalHook<HookFunc::Global_GetLocalPlayerIndex>();
	InitGlobalHook<HookFunc::Global_GetVectorInScreenSpace>();
	InitGlobalHook<HookFunc::Global_UserInfoChangedCallback>();
	InitGlobalHook<HookFunc::Global_UTILComputeEntityFade>();
	InitGlobalHook<HookFunc::Global_UTIL_TraceLine>();

	InitGlobalHook<HookFunc::CModelLoader_Map_LoadModel>();
	InitGlobalHook<HookFunc::Global_Map_CheckForHDR>();
	InitGlobalHook<HookFunc::Global_EnableHDR>();
	InitGlobalHook<HookFunc::CTextureManager_LoadTexture>();
}