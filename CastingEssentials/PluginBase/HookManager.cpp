#include "HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Exceptions.h"
#include "Controls/StubPanel.h"

#include <PolyHook.h>

#include <Windows.h>
#include <Psapi.h>
#include <convar.h>
#include <icvar.h>
#include <cdll_int.h>
#include <client/hltvcamera.h>

static HookManager s_HookManager;
HookManager* GetHooks() { return &s_HookManager; }

class HookManager::Panel final : vgui::StubPanel
{
public:
	Panel()
	{
		m_InGame = false;
	}
	void OnTick() override
	{
		if (!m_InGame && Interfaces::GetEngineClient()->IsInGame())
		{
			m_InGame = true;
			s_HookManager.IngameStateChanged(m_InGame);
		}
		else if (m_InGame && Interfaces::GetEngineClient()->IsInGame())
		{
			m_InGame = false;
			s_HookManager.IngameStateChanged(m_InGame);
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

	return FindPattern((DWORD)clientModule, clientModInfo.SizeOfImage, (PBYTE)signature, mask);
}

C_HLTVCamera* HookManager::GetHLTVCamera()
{
	return Interfaces::GetHLTVCamera();
}

HookManager::RawSetCameraAngleFn HookManager::GetRawFunc_C_HLTVCamera_SetCameraAngle()
{
	static RawSetCameraAngleFn s_SetCameraAngleFn = nullptr;
	if (!s_SetCameraAngleFn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x8B\x45\x08\x56\x8B\xF1\x8D\x56\x00\xD9\x00\xD9\x1A\xD9\x40\x00\xD9\x5A\x00\xD9\x40\x00\x52";
		constexpr const char* MASK = "xxxxxxxxxxx?xxxxxx?xx?xx?x";

		s_SetCameraAngleFn = (RawSetCameraAngleFn)SignatureScan("client", SIG, MASK);

		if (!s_SetCameraAngleFn)
			throw bad_pointer("C_HLTVCamera::SetCameraAngle");
	}

	return s_SetCameraAngleFn;
}

HookManager::RawSetModeFn HookManager::GetRawFunc_C_HLTVCamera_SetMode()
{
	static RawSetModeFn s_SetModeFn = nullptr;
	if (!s_SetModeFn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x8B\x45\x08\x53\x56\x8B\xF1\x8B\x5E\x00";
		constexpr const char* MASK = "xxxxxxxxxxxx?";

		s_SetModeFn = (RawSetModeFn)SignatureScan("client", SIG, MASK);

		if (!s_SetModeFn)
			throw bad_pointer("C_HLTVCamera::SetMode");
	}

	return s_SetModeFn;
}

HookManager::RawSetPrimaryTargetFn HookManager::GetRawFunc_C_HLTVCamera_SetPrimaryTarget()
{
	static RawSetPrimaryTargetFn s_SetPrimaryTargetFn = nullptr;
	if (!s_SetPrimaryTargetFn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x8B\x45\x08\x83\xEC\x00\x53\x56\x8B\xF1";
		constexpr const char* MASK = "xxxxxxxx?xxxx";

		s_SetPrimaryTargetFn = (RawSetPrimaryTargetFn)SignatureScan("client", SIG, MASK);

		if (!s_SetPrimaryTargetFn)
			throw bad_pointer("C_HLTVCamera::SetPrimaryTarget");
	}

	return s_SetPrimaryTargetFn;
}

HookManager::RawGetLocalPlayerIndexFn HookManager::GetRawFunc_Global_GetLocalPlayerIndex()
{
	static RawGetLocalPlayerIndexFn s_GetLocalPlayerIndexFn = nullptr;
	if (!s_GetLocalPlayerIndexFn)
	{
		constexpr const char* SIG = "\xE8\x00\x00\x00\x00\x85\xC0\x74\x08\x8D\x48\x08\x8B\x01\xFF\x60\x24\x33\xC0\xC3";
		constexpr const char* MASK = "x????xxxxxxxxxxxxxxx";

		s_GetLocalPlayerIndexFn = (RawGetLocalPlayerIndexFn)SignatureScan("client", SIG, MASK);

		if (!s_GetLocalPlayerIndexFn)
			throw bad_pointer("C_HLTVCamera::SetPrimaryTarget");
	}

	return s_GetLocalPlayerIndexFn;
}

void HookManager::IngameStateChanged(bool inGame)
{
	if (inGame)
	{
		m_Hook_IGameEventManager2_FireEventClientSide.reset(new IGameEventManager2_FireEventClientSide(Interfaces::GetGameEventManager(), &IGameEventManager2::FireEventClientSide));
	}
	else
	{
		m_Hook_IGameEventManager2_FireEventClientSide.reset();
	}
}

bool HookManager::Load()
{
	m_Panel.reset(new Panel());
	m_Hook_ICvar_ConsoleColorPrintf.reset(new ICvar_ConsoleColorPrintf(g_pCVar, &ICvar::ConsoleColorPrintf));
	m_Hook_ICvar_ConsoleDPrintf.reset(new ICvar_ConsoleDPrintf(g_pCVar, &ICvar::ConsoleDPrintf));
	m_Hook_ICvar_ConsolePrintf.reset(new ICvar_ConsolePrintf(g_pCVar, &ICvar::ConsolePrintf));

	m_Hook_IVEngineClient_GetPlayerInfo.reset(new IVEngineClient_GetPlayerInfo(Interfaces::GetEngineClient(), &IVEngineClient::GetPlayerInfo));

	m_Hook_C_HLTVCamera_SetCameraAngle.reset(new C_HLTVCamera_SetCameraAngle(Interfaces::GetHLTVCamera(), GetRawFunc_C_HLTVCamera_SetCameraAngle()));
	m_Hook_C_HLTVCamera_SetMode.reset(new C_HLTVCamera_SetMode(Interfaces::GetHLTVCamera(), GetRawFunc_C_HLTVCamera_SetMode()));
	m_Hook_C_HLTVCamera_SetPrimaryTarget.reset(new C_HLTVCamera_SetPrimaryTarget(Interfaces::GetHLTVCamera(), GetRawFunc_C_HLTVCamera_SetPrimaryTarget()));

	m_Hook_Global_GetLocalPlayerIndex.reset(new Global_GetLocalPlayerIndex(GetRawFunc_Global_GetLocalPlayerIndex()));

	return true;
}
bool HookManager::Unload()
{
	m_Panel.reset();
	//g_SourceHook.UnloadPlugin(g_PLID, new StatusSpecUnloader());
	m_Hook_ICvar_ConsoleColorPrintf.reset();
	m_Hook_ICvar_ConsoleDPrintf.reset();
	m_Hook_ICvar_ConsolePrintf.reset();

	m_Hook_IVEngineClient_GetPlayerInfo.reset();

	m_Hook_IGameEventManager2_FireEventClientSide.reset();

	m_Hook_C_HLTVCamera_SetCameraAngle.reset();
	m_Hook_C_HLTVCamera_SetMode.reset();
	m_Hook_C_HLTVCamera_SetPrimaryTarget.reset();

	m_Hook_Global_GetLocalPlayerIndex.reset();

	return true;
}