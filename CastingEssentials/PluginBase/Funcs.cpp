#include "Funcs.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Exceptions.h"

#include <PolyHook.h>

#include <Windows.h>
#include <Psapi.h>
#include <convar.h>
#include <icvar.h>
#include <cdll_int.h>
#include <client/hltvcamera.h>

std::unique_ptr<Funcs::Hook_ICvar_ConsoleColorPrintf> Funcs::s_Hook_ICvar_ConsoleColorPrintf;
std::unique_ptr<Funcs::Hook_ICvar_ConsoleDPrintf> Funcs::s_Hook_ICvar_ConsoleDPrintf;
std::unique_ptr<Funcs::Hook_ICvar_ConsolePrintf> Funcs::s_Hook_ICvar_ConsolePrintf;

std::unique_ptr<Funcs::Hook_IVEngineClient_GetPlayerInfo> Funcs::s_Hook_IVEngineClient_GetPlayerInfo;

std::unique_ptr<Funcs::Hook_IGameEventManager2_FireEventClientSide> Funcs::s_Hook_IGameEventManager2_FireEventClientSide;

std::unique_ptr<Funcs::Hook_C_HLTVCamera_SetCameraAngle> Funcs::s_Hook_C_HLTVCamera_SetCameraAngle;
std::unique_ptr<Funcs::Hook_C_HLTVCamera_SetMode> Funcs::s_Hook_C_HLTVCamera_SetMode;
std::unique_ptr<Funcs::Hook_C_HLTVCamera_SetPrimaryTarget> Funcs::s_Hook_C_HLTVCamera_SetPrimaryTarget;

std::unique_ptr<Funcs::Hook_Global_GetLocalPlayerIndex> Funcs::s_Hook_Global_GetLocalPlayerIndex;

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

Funcs::RawSetCameraAngleFn Funcs::GetRawFunc_C_HLTVCamera_SetCameraAngle()
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

Funcs::Hook_C_HLTVCamera_SetCameraAngle::Functional Funcs::GetFunc_C_HLTVCamera_SetCameraAngle()
{
	return std::bind(
		[](RawSetCameraAngleFn func, C_HLTVCamera* pThis, const QAngle& ang) { func(pThis, ang); },
		GetRawFunc_C_HLTVCamera_SetCameraAngle(), Interfaces::GetHLTVCamera(), std::placeholders::_1);
}

Funcs::RawSetModeFn Funcs::GetRawFunc_C_HLTVCamera_SetMode()
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

Funcs::Hook_C_HLTVCamera_SetMode::Functional Funcs::GetFunc_C_HLTVCamera_SetMode()
{
	return std::bind(
		[](RawSetModeFn func, C_HLTVCamera* pThis, int mode) { func(pThis, mode); },
		GetRawFunc_C_HLTVCamera_SetMode(), Interfaces::GetHLTVCamera(), std::placeholders::_1);
}

Funcs::RawSetPrimaryTargetFn Funcs::GetRawFunc_C_HLTVCamera_SetPrimaryTarget()
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

Funcs::RawGetLocalPlayerIndexFn Funcs::GetRawFunc_Global_GetLocalPlayerIndex()
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

Funcs::Hook_Global_GetLocalPlayerIndex::Functional Funcs::GetFunc_Global_GetLocalPlayerIndex()
{
	return std::bind(GetRawFunc_Global_GetLocalPlayerIndex());
}

Funcs::Hook_C_HLTVCamera_SetPrimaryTarget::Functional Funcs::GetFunc_C_HLTVCamera_SetPrimaryTarget()
{
	return std::bind(
		[](RawSetPrimaryTargetFn func, C_HLTVCamera* pThis, int target) { func(pThis, target); },
		GetRawFunc_C_HLTVCamera_SetPrimaryTarget(), Interfaces::GetHLTVCamera(), std::placeholders::_1);
}

bool Funcs::Load()
{
	s_Hook_ICvar_ConsoleColorPrintf.reset(new Hook_ICvar_ConsoleColorPrintf(g_pCVar, &ICvar::ConsoleColorPrintf));
	s_Hook_ICvar_ConsoleDPrintf.reset(new Hook_ICvar_ConsoleDPrintf(g_pCVar, &ICvar::ConsoleDPrintf));
	s_Hook_ICvar_ConsolePrintf.reset(new Hook_ICvar_ConsolePrintf(g_pCVar, &ICvar::ConsolePrintf));

	s_Hook_IVEngineClient_GetPlayerInfo.reset(new Hook_IVEngineClient_GetPlayerInfo(Interfaces::GetEngineClient(), &IVEngineClient::GetPlayerInfo));

	//s_Hook_IGameEventManager2_FireEventClientSide.reset(new Hook_IGameEventManager2_FireEventClientSide(Interfaces::GetGameEventManager(), &IGameEventManager2::FireEventClientSide));

	s_Hook_C_HLTVCamera_SetCameraAngle.reset(new Hook_C_HLTVCamera_SetCameraAngle(Interfaces::GetHLTVCamera(), GetRawFunc_C_HLTVCamera_SetCameraAngle()));
	s_Hook_C_HLTVCamera_SetMode.reset(new Hook_C_HLTVCamera_SetMode(Interfaces::GetHLTVCamera(), GetRawFunc_C_HLTVCamera_SetMode()));
	s_Hook_C_HLTVCamera_SetPrimaryTarget.reset(new Hook_C_HLTVCamera_SetPrimaryTarget(Interfaces::GetHLTVCamera(), GetRawFunc_C_HLTVCamera_SetPrimaryTarget()));

	s_Hook_Global_GetLocalPlayerIndex.reset(new Hook_Global_GetLocalPlayerIndex(GetRawFunc_Global_GetLocalPlayerIndex()));

	return true;
}
bool Funcs::Unload()
{
	//g_SourceHook.UnloadPlugin(g_PLID, new StatusSpecUnloader());
	s_Hook_ICvar_ConsoleColorPrintf.reset();
	s_Hook_ICvar_ConsoleDPrintf.reset();
	s_Hook_ICvar_ConsolePrintf.reset();

	s_Hook_IVEngineClient_GetPlayerInfo.reset();

	s_Hook_C_HLTVCamera_SetCameraAngle.reset();
	s_Hook_C_HLTVCamera_SetMode.reset();
	s_Hook_C_HLTVCamera_SetPrimaryTarget.reset();

	return true;
}