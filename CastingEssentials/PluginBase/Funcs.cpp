#include "Funcs.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Exceptions.h"
#include "PluginBase/EZHook.h"

#include "MinHook.h"

#include <PolyHook.h>

#include <Windows.h>
#include <Psapi.h>
#include <convar.h>
#include <icvar.h>
#include <cdll_int.h>
#include <client/hltvcamera.h>

Funcs::SetModeFn Funcs::m_SetModeOriginal = nullptr;
int Funcs::m_SetModeLastHookRegistered = 0;
std::map<int, std::function<void(C_HLTVCamera *, int &)>> Funcs::m_SetModeHooks;

std::unique_ptr<Funcs::Hook_ICvar_ConsoleColorPrintf> Funcs::s_Hook_ICvar_ConsoleColorPrintf;

//int Funcs::m_SetModelLastHookRegistered = 0;
//std::map<int, std::function<void(C_BaseEntity *, const model_t *&)>> Funcs::m_SetModelHooks;

Funcs::SetPrimaryTargetFn Funcs::m_SetPrimaryTargetOriginal = nullptr;
int Funcs::m_SetPrimaryTargetLastHookRegistered = 0;
std::map<int, std::function<void(C_HLTVCamera *, int &)>> Funcs::m_SetPrimaryTargetHooks;

std::shared_ptr<PLH::IHook> Funcs::s_BaseHooks[Funcs::Func::Count];
std::recursive_mutex Funcs::s_BaseHooksMutex;

std::atomic<uint64> Funcs::s_LastHook;
std::recursive_mutex Funcs::s_HooksTableMutex;
std::map<unsigned int, std::shared_ptr<PLH::IHook>> Funcs::s_HooksTable;

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

Funcs::SetCameraAngleFn Funcs::s_SetCameraAngleFn = nullptr;
Funcs::SetModeFn Funcs::s_SetModeFn = nullptr;
Funcs::SetPrimaryTargetFn Funcs::s_SetPrimaryTargetFn = nullptr;

Funcs::IVirtualHook* Funcs::GetVHook(Func fn)
{
	switch (fn)
	{
		case Func::ICvar_ConsoleColorPrintf:	return s_Hook_ICvar_ConsoleColorPrintf.get();
	}

	return nullptr;
}

Funcs::SetCameraAngleFn Funcs::GetFunc_C_HLTVCamera_SetCameraAngle()
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

Funcs::SetModeFn Funcs::GetFunc_C_HLTVCamera_SetMode()
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

Funcs::SetPrimaryTargetFn Funcs::GetFunc_C_HLTVCamera_SetPrimaryTarget()
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

int Funcs::AddHook_ICvar_ConsoleDPrintf(ICvar * instance, const ICvar_ConsoleDPrintfFn & hook)
{
	return ++s_LastHook;
}

int Funcs::AddHook_ICvar_ConsolePrintf(ICvar * instance, const ICvar_ConsolePrintfFn & hook)
{
	return ++s_LastHook;
}

int Funcs::AddHook_IVEngineClient_GetPlayerInfo(IVEngineClient * instance, const IVEngineClient_GetPlayerInfoFn & hook)
{
	return ++s_LastHook;
}

int Funcs::AddHook_C_HLTVCamera_SetMode(const C_HLTVCamera_SetModeFn & hook)
{
	return ++s_LastHook;
}

int Funcs::AddHook_C_HLTVCamera_SetPrimaryTarget(const C_HLTVCamera_SetPrimaryTargetFn & hook)
{
	return ++s_LastHook;
}

bool Funcs::Load()
{
	s_Hook_ICvar_ConsoleColorPrintf.reset(new Hook_ICvar_ConsoleColorPrintf(g_pCVar, &ICvar::ConsoleColorPrintf));

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

Funcs::ICvar_ConsoleDPrintfFn Funcs::Original_ICvar_ConsoleDPrintf()
{
	auto hook = s_BaseHooks[Func::ICvar_ConsoleDPrintf];
	if (hook)
	{
		Assert(0);
	}
	else
		return std::bind(&ICvar::ConsoleDPrintf, g_pCVar, std::placeholders::_1);
}

Funcs::ICvar_ConsolePrintfFn Funcs::Original_ICvar_ConsolePrintf()
{
	auto hook = s_BaseHooks[Func::ICvar_ConsolePrintf];
	if (hook)
	{
		Assert(0);
	}
	else
		return std::bind(&ICvar::ConsolePrintf, g_pCVar, std::placeholders::_1);
}

Funcs::IVEngineClient_GetPlayerInfoFn Funcs::Original_IVEngineClient_GetPlayerInfo()
{
	auto hook = s_BaseHooks[Func::IVEngineClient_GetPlayerInfo];
	if (hook)
	{
		Assert(0);
	}
	else
		return std::bind(&IVEngineClient::GetPlayerInfo, Interfaces::GetEngineClient(), std::placeholders::_1, std::placeholders::_2);
}

Funcs::C_HLTVCamera_SetModeFn Funcs::Original_C_HLTVCamera_SetMode()
{
	auto hook = s_BaseHooks[Func::C_HLTVCamera_SetMode];
	if (hook)
	{
		Assert(0);
	}
	else
	{
		auto rawFn = GetFunc_C_HLTVCamera_SetMode();
		Assert(rawFn);

		auto cam = Interfaces::GetHLTVCamera();
		Assert(cam);

		return C_HLTVCamera_SetModeFn([rawFn, cam](int i) { rawFn(cam, i); });
	}
}

Funcs::C_HLTVCamera_SetPrimaryTargetFn Funcs::Original_C_HLTVCamera_SetPrimaryTarget()
{
	auto hook = s_BaseHooks[Func::C_HLTVCamera_SetPrimaryTarget];
	if (hook)
	{
		Assert(0);
	}
	else
	{
		auto rawFn = GetFunc_C_HLTVCamera_SetPrimaryTarget();
		Assert(rawFn);

		auto cam = Interfaces::GetHLTVCamera();
		Assert(cam);

		return C_HLTVCamera_SetPrimaryTargetFn([rawFn, cam](int i) { rawFn(cam, i); });
	}
}

void* Funcs::GetOriginalRawFn(const std::shared_ptr<PLH::IHook>& hook)
{
	PLH::VFuncSwap* realHook = assert_cast<PLH::VFuncSwap*>(hook.get());
	return realHook->GetOriginal<void*>();
}

std::shared_ptr<PLH::IHook> Funcs::SetupHook(BYTE ** instance, int vtableOffset, BYTE * detourFunc)
{
	PLH::VFuncSwap* newHook = new PLH::VFuncSwap();
	newHook->SetupHook(instance, vtableOffset, detourFunc);
	if (!newHook->Hook())
	{
		Assert(0);
		return nullptr;
	}

	return std::shared_ptr<PLH::IHook>(newHook);
}

bool Funcs::RemoveHook(int hookID, const char* funcName)
{
	std::lock_guard<std::recursive_mutex> scopeLock(s_HooksTableMutex);

	auto found = s_HooksTable.find(hookID);
	if (found == s_HooksTable.end())
	{
		PluginWarning("Function %s called %s with invalid hook ID %i!\n", funcName, __FUNCSIG__, hookID);
		return false;
	}

	auto hook = found->second;
	hook->UnHook();

	s_HooksTable.erase(found);

	return true;
}

void Funcs::RemoveHook_C_HLTVCamera_SetMode(int hookID)
{
	m_SetModeHooks.erase(hookID);

	if (m_SetModeHooks.size() == 0)
		RemoveDetour_C_HLTVCamera_SetMode();
}

void Funcs::RemoveHook_C_HLTVCamera_SetPrimaryTarget(int hookID)
{
	m_SetPrimaryTargetHooks.erase(hookID);

	if (m_SetPrimaryTargetHooks.size() == 0)
		RemoveDetour_C_HLTVCamera_SetPrimaryTarget();
}

bool Funcs::AddDetour_C_HLTVCamera_SetMode(SetModeDetourFn detour)
{
	void *original;

	if (AddDetour(GetFunc_C_HLTVCamera_SetMode(), detour, original))
	{
		m_SetModeOriginal = reinterpret_cast<SetModeFn>(original);
		return true;
	}

	return false;
}

bool Funcs::AddDetour_C_HLTVCamera_SetPrimaryTarget(SetPrimaryTargetDetourFn detour)
{
	void *original;

	if (AddDetour(GetFunc_C_HLTVCamera_SetPrimaryTarget(), detour, original))
	{
		m_SetPrimaryTargetOriginal = reinterpret_cast<SetPrimaryTargetFn>(original);
		return true;
	}

	return false;
}

bool Funcs::AddDetour(void *target, void *detour, void *&original)
{
	MH_STATUS addHookResult = MH_CreateHook(target, detour, &original);

	if (addHookResult != MH_OK)
	{
		return false;
	}

	MH_STATUS enableHookResult = MH_EnableHook(target);

	return (enableHookResult == MH_OK);
}

void Funcs::Detour_C_HLTVCamera_SetMode(C_HLTVCamera *instance, void *, int iMode)
{
	for (auto iterator : m_SetModeHooks)
	{
		iterator.second(instance, iMode);
	}

	Funcs::Original_C_HLTVCamera_SetMode()(iMode);
}

void Funcs::Detour_C_HLTVCamera_SetPrimaryTarget(C_HLTVCamera *instance, void *, int nEntity)
{
	for (auto iterator : m_SetPrimaryTargetHooks)
		iterator.second(instance, nEntity);

	Funcs::Original_C_HLTVCamera_SetPrimaryTarget()(nEntity);
}

bool Funcs::RemoveDetour_C_HLTVCamera_SetMode()
{
	if (RemoveDetour(GetFunc_C_HLTVCamera_SetMode()))
	{
		m_SetModeOriginal = nullptr;
		return true;
	}

	return false;
}

bool Funcs::RemoveDetour_C_HLTVCamera_SetPrimaryTarget()
{
	if (RemoveDetour(GetFunc_C_HLTVCamera_SetPrimaryTarget()))
	{
		m_SetPrimaryTargetOriginal = nullptr;
		return true;
	}

	return false;
}

bool Funcs::RemoveDetour(void *target)
{
	MH_STATUS disableHookResult = MH_DisableHook(target);
	if (disableHookResult != MH_OK)
		return false;

	MH_STATUS removeHookResult = MH_RemoveHook(target);
	return (removeHookResult == MH_OK);
}

int Funcs::MFI_GetVTblOffset(void * mfp)
{
	static_assert(_MSC_VER == 1900, "Only verified on VS2015!");

	unsigned char *addr = (unsigned char*)mfp;
	if (*addr == 0xE9)		// Jmp
	{
		// May or may not be!
		// Check where it'd jump
		addr += 5 /*size of the instruction*/ + *(unsigned long*)(addr + 1);
	}

	// Check whether it's a virtual function call
	// They look like this:
	// 004125A0 8B 01            mov         eax,dword ptr [ecx] 
	// 004125A2 FF 60 04         jmp         dword ptr [eax+4]
	//		==OR==
	// 00411B80 8B 01            mov         eax,dword ptr [ecx] 
	// 00411B82 FF A0 18 03 00 00 jmp         dword ptr [eax+318h]

	// However, for vararg functions, they look like this:
	// 0048F0B0 8B 44 24 04      mov         eax,dword ptr [esp+4]
	// 0048F0B4 8B 00            mov         eax,dword ptr [eax]
	// 0048F0B6 FF 60 08         jmp         dword ptr [eax+8]
	//		==OR==
	// 0048F0B0 8B 44 24 04      mov         eax,dword ptr [esp+4]
	// 0048F0B4 8B 00            mov         eax,dword ptr [eax]
	// 00411B82 FF A0 18 03 00 00 jmp         dword ptr [eax+318h]

	// With varargs, the this pointer is passed as if it was the first argument

	bool ok = false;
	if (addr[0] == 0x8B && addr[1] == 0x44 && addr[2] == 0x24 && addr[3] == 0x04 &&
		addr[4] == 0x8B && addr[5] == 0x00)
	{
		addr += 6;
		ok = true;
	}
	else if (addr[0] == 0x8B && addr[1] == 0x01)
	{
		addr += 2;
		ok = true;
	}
	if (!ok)
		return -1;

	if (*addr++ == 0xFF)
	{
		if (*addr == 0x60)
		{
			return *++addr / 4;
		}
		else if (*addr == 0xA0)
		{
			return *((unsigned int*)++addr) / 4;
		}
		else if (*addr == 0x20)
			return 0;
		else
			return -1;
	}
	return -1;
}
