#include "Funcs.h"
#include "PluginBase/Exceptions.h"

#include "MinHook.h"

#include <sourcehook_impl.h>

#include <Windows.h>
#include <Psapi.h>
#include <convar.h>
#include <icvar.h>

SourceHook::Impl::CSourceHookImpl g_SourceHook;
SourceHook::ISourceHook *g_SHPtr = &g_SourceHook;
int g_PLID = 0;

SH_DECL_HOOK1_void_vafmt(ICvar, ConsoleColorPrintf, const FMTFUNCTION(3, 4), 0, const Color &);
SH_DECL_HOOK0_void_vafmt(ICvar, ConsolePrintf, const FMTFUNCTION(2, 3), 0);
SH_DECL_HOOK0_void_vafmt(ICvar, ConsoleDPrintf, const FMTFUNCTION(2, 3), 0);

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

SetCameraAngleFn Funcs::s_SetCameraAngleFn = nullptr;
SetModeFn Funcs::s_SetModeFn = nullptr;
SetPrimaryTargetFn Funcs::s_SetPrimaryTargetFn = nullptr;

SetCameraAngleFn Funcs::GetFunc_C_HLTVCamera_SetCameraAngle()
{
	if (!s_SetCameraAngleFn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x8B\x45\x08\x56\x8B\xF1\x8D\x56\x00\xD9\x00\xD9\x1A\xD9\x40\x00\xD9\x5A\x00\xD9\x40\x00\x52";
		constexpr const char* MASK = "xxxxxxxxxxx?xxxxxx?xx?xx?x";

		s_SetCameraAngleFn = (SetCameraAngleFn)SignatureScan("client", SIG, MASK);
		
		if (!s_SetCameraAngleFn)
			throw bad_pointer("C_HLTVCamera::SetCameraAngle");
	}

	return s_SetCameraAngleFn;
}

SetModeFn Funcs::GetFunc_C_HLTVCamera_SetMode()
{
	if (!s_SetModeFn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x8B\x45\x08\x53\x56\x8B\xF1\x8B\x5E\x00";
		constexpr const char* MASK = "xxxxxxxxxxxx?";

		s_SetModeFn = (SetModeFn)SignatureScan("client", SIG, MASK);

		if (!s_SetModeFn)
			throw bad_pointer("C_HLTVCamera::SetMode");
	}

	return s_SetModeFn;
}

SetPrimaryTargetFn Funcs::GetFunc_C_HLTVCamera_SetPrimaryTarget()
{
	if (!s_SetPrimaryTargetFn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x8B\x45\x08\x83\xEC\x00\x53\x56\x8B\xF1";
		constexpr const char* MASK = "xxxxxxxx?xxxx";

		s_SetPrimaryTargetFn = (SetPrimaryTargetFn)SignatureScan("client", SIG, MASK);

		if (!s_SetPrimaryTargetFn)
			throw bad_pointer("C_HLTVCamera::SetPrimaryTarget");
	}

	return s_SetPrimaryTargetFn;
}

bool Funcs::Load()
{
	MH_STATUS minHookResult = MH_Initialize();

	return (minHookResult == MH_OK || minHookResult == MH_ERROR_ALREADY_INITIALIZED);
}
bool Funcs::Unload()
{
	s_SetCameraAngleFn = nullptr;
	s_SetModeFn = nullptr;
	s_SetPrimaryTargetFn = nullptr;

	//g_SourceHook.UnloadPlugin(g_PLID, new StatusSpecUnloader());
	MH_STATUS minHookResult = MH_Uninitialize();

	return (minHookResult == MH_OK || minHookResult == MH_ERROR_NOT_INITIALIZED);
}

bool Funcs::RemoveHook(int hookID, const char* funcName)
{
	bool result = SH_REMOVE_HOOK_ID(hookID);
	if (!result)
		PluginWarning("Unable to remove hookID %i from within %s!\n", hookID, funcName);
	
	return result;
}

int Funcs::AddHook_ICvar_ConsoleColorPrintf(ICvar *instance, fastdelegate::FastDelegate<void, const Color &, const char *> hook, bool post)
{
	return SH_ADD_HOOK(ICvar, ConsoleColorPrintf, instance, hook, post);
}

int Funcs::AddHook_ICvar_ConsoleDPrintf(ICvar *instance, fastdelegate::FastDelegate<void, const char *> hook, bool post)
{
	return SH_ADD_HOOK(ICvar, ConsoleDPrintf, instance, hook, post);
}

int Funcs::AddHook_ICvar_ConsolePrintf(ICvar *instance, fastdelegate::FastDelegate<void, const char *> hook, bool post)
{
	return SH_ADD_HOOK(ICvar, ConsolePrintf, instance, hook, post);
}