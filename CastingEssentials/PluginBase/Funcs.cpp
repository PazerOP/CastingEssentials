#include "Funcs.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Exceptions.h"
#include "PluginBase/EZHook.h"

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

Funcs::Func_C_HLTVCamera_SetCameraAngle Funcs::GetFunc_C_HLTVCamera_SetCameraAngle()
{
	typedef void(__thiscall *RawSetCameraAngleFn)(C_HLTVCamera*, const QAngle&);
	static RawSetCameraAngleFn s_SetCameraAngleFn = nullptr;

	if (!s_SetCameraAngleFn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x8B\x45\x08\x56\x8B\xF1\x8D\x56\x00\xD9\x00\xD9\x1A\xD9\x40\x00\xD9\x5A\x00\xD9\x40\x00\x52";
		constexpr const char* MASK = "xxxxxxxxxxx?xxxxxx?xx?xx?x";

		s_SetCameraAngleFn = (RawSetCameraAngleFn)SignatureScan("client", SIG, MASK);

		if (!s_SetCameraAngleFn)
			throw bad_pointer("C_HLTVCamera::SetCameraAngle");
	}

	return std::bind(
		[](RawSetCameraAngleFn func, C_HLTVCamera* pThis, const QAngle& ang) { func(pThis, ang); },
		s_SetCameraAngleFn, Interfaces::GetHLTVCamera(), std::placeholders::_1);
}

Funcs::Func_C_HLTVCamera_SetMode Funcs::GetFunc_C_HLTVCamera_SetMode()
{
	typedef void(__thiscall *RawSetModeFn)(C_HLTVCamera*, int);
	static RawSetModeFn s_SetModeFn = nullptr;

	if (!s_SetModeFn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x8B\x45\x08\x53\x56\x8B\xF1\x8B\x5E\x00";
		constexpr const char* MASK = "xxxxxxxxxxxx?";

		s_SetModeFn = (RawSetModeFn)SignatureScan("client", SIG, MASK);

		if (!s_SetModeFn)
			throw bad_pointer("C_HLTVCamera::SetMode");
	}

	return std::bind(
		[](RawSetModeFn func, C_HLTVCamera* pThis, int mode) { func(pThis, mode); },
		s_SetModeFn, Interfaces::GetHLTVCamera(), std::placeholders::_1);
}

Funcs::Func_C_HLTVCamera_SetPrimaryTarget Funcs::GetFunc_C_HLTVCamera_SetPrimaryTarget()
{
	typedef void(__thiscall *RawSetPrimaryTargetFn)(C_HLTVCamera*, int);
	static RawSetPrimaryTargetFn s_SetPrimaryTargetFn = nullptr;

	if (!s_SetPrimaryTargetFn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x8B\x45\x08\x83\xEC\x00\x53\x56\x8B\xF1";
		constexpr const char* MASK = "xxxxxxxx?xxxx";

		s_SetPrimaryTargetFn = (RawSetPrimaryTargetFn)SignatureScan("client", SIG, MASK);

		if (!s_SetPrimaryTargetFn)
			throw bad_pointer("C_HLTVCamera::SetPrimaryTarget");
	}

	return std::bind(
		[](RawSetPrimaryTargetFn func, C_HLTVCamera* pThis, int target) { func(pThis, target); },
		s_SetPrimaryTargetFn, Interfaces::GetHLTVCamera(), std::placeholders::_1);
}

bool Funcs::Load()
{
	s_Hook_ICvar_ConsoleColorPrintf.reset(new Hook_ICvar_ConsoleColorPrintf(g_pCVar, &ICvar::ConsoleColorPrintf));
	s_Hook_ICvar_ConsoleDPrintf.reset(new Hook_ICvar_ConsoleDPrintf(g_pCVar, &ICvar::ConsoleDPrintf));
	s_Hook_ICvar_ConsolePrintf.reset(new Hook_ICvar_ConsolePrintf(g_pCVar, &ICvar::ConsolePrintf));

	s_Hook_IVEngineClient_GetPlayerInfo.reset(new Hook_IVEngineClient_GetPlayerInfo(Interfaces::GetEngineClient(), &IVEngineClient::GetPlayerInfo));

	return true;
}
bool Funcs::Unload()
{
	//g_SourceHook.UnloadPlugin(g_PLID, new StatusSpecUnloader());
	s_Hook_ICvar_ConsoleColorPrintf.reset();
	s_Hook_ICvar_ConsoleDPrintf.reset();
	s_Hook_ICvar_ConsolePrintf.reset();

	s_Hook_IVEngineClient_GetPlayerInfo.reset();

	return true;
}