#include "HookManager.h"
#include "Controls/StubPanel.h"
#include "Misc/HLTVCameraHack.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Exceptions.h"

#include <PolyHook.hpp>

#include <Windows.h>
#include <Psapi.h>
#include <convar.h>
#include <icvar.h>
#include <cdll_int.h>
#include <client/hltvcamera.h>
#include <toolframework/iclientenginetools.h>
#include <iprediction.h>

static std::unique_ptr<HookManager> s_HookManager;
HookManager* GetHooks() { Assert(s_HookManager); return s_HookManager.get(); }

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

HookManager::RawGetBoneCacheFn HookManager::GetRawFunc_C_BaseAnimating_GetBoneCache()
{
	static RawGetBoneCacheFn s_GetBoneCacheFn = nullptr;
	if (!s_GetBoneCacheFn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x83\xEC\x10\x56\x8B\xF1\x57\xFF\xB6";
		constexpr const char* MASK = "xxxxxxxxxxxx";

		s_GetBoneCacheFn = (RawGetBoneCacheFn)SignatureScan("client", SIG, MASK);

		if (!s_GetBoneCacheFn)
			throw bad_pointer("C_BaseAnimating::GetBoneCache");
	}

	return s_GetBoneCacheFn;
}

HookManager::RawLockStudioHdrFn HookManager::GetRawFunc_C_BaseAnimating_LockStudioHdr()
{
	static RawLockStudioHdrFn s_LockStudioHdrFn = nullptr;
	if (!s_LockStudioHdrFn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x83\xEC\x20\x56\x57\x6A\x01\x68\x00\x00\x00\x00\x8B\xF1";
		constexpr const char* MASK = "xxxxxxxxxxx????xx";

		s_LockStudioHdrFn = (RawLockStudioHdrFn)SignatureScan("client", SIG, MASK);

		if (!s_LockStudioHdrFn)
			throw bad_pointer("C_BaseAnimating::LockStudioHdr");
	}

	return s_LockStudioHdrFn;
}

HookManager::RawCalcAbsolutePositionFn HookManager::GetRawFunc_C_BaseEntity_CalcAbsolutePosition()
{
	static RawCalcAbsolutePositionFn s_CalcAbsolutePositionFn = nullptr;
	if (!s_CalcAbsolutePositionFn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x81\xEC\x00\x00\x00\x00\x80\x3D\x00\x00\x00\x00\x00\x53\x8B\xD9\x0F\x84";
		constexpr const char* MASK = "xxxxx????xx?????xxxxx";

		s_CalcAbsolutePositionFn = (RawCalcAbsolutePositionFn)SignatureScan("client", SIG, MASK);

		if (!s_CalcAbsolutePositionFn)
			throw bad_pointer("C_BaseEntity::CalcAbsolutePosition");
	}

	return s_CalcAbsolutePositionFn;
}

HookManager::RawLookupBoneFn HookManager::GetRawFunc_C_BaseAnimating_LookupBone()
{
	static RawLookupBoneFn s_LookupBoneFn = nullptr;
	if (!s_LookupBoneFn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x56\x8B\xF1\x80\xBE\x00\x00\x00\x00\x00\x74\x13\xFF\x75\x08\x33\xC0\x50\xE8\x00\x00\x00\x00\x83\xC4\x08\x5E\x5D\xC2\x04\x00\x83\xBE\x00\x00\x00\x00\x00\x75\x05\xE8\x00\x00\x00\x00\xFF\x75\x08\x8B\x86\x00\x00\x00\x00\x50\xE8\x00\x00\x00\x00\x83\xC4\x08\x5E\x5D\xC2\x04\x00";
		constexpr const char* MASK = "xxxxxxxx?????xxxxxxxxx????xxxxxxxxxx?????xxx????xxxxx????xx????xxxxxxxx";

		s_LookupBoneFn = (RawLookupBoneFn)SignatureScan("client", SIG, MASK);

		if (!s_LookupBoneFn)
			throw bad_pointer("C_BaseAnimating::LookupBone");
	}

	return s_LookupBoneFn;
}

HookManager::RawGetBonePositionFn HookManager::GetRawFunc_C_BaseAnimating_GetBonePosition()
{
	static RawGetBonePositionFn s_GetBonePositionFn = nullptr;
	if (!s_GetBonePositionFn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x83\xEC\x30\x56\x6A\x00";
		constexpr const char* MASK = "xxxxxxxxx";

		s_GetBonePositionFn = (RawGetBonePositionFn)SignatureScan("client", SIG, MASK);

		if (!s_GetBonePositionFn)
			throw bad_pointer("C_BaseAnimating::GetBoneTransform");
	}

	return s_GetBonePositionFn;
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
			throw bad_pointer("GetLocalPlayerIndex");
	}

	return s_GetLocalPlayerIndexFn;
}

HookManager::RawCreateEntityByNameFn HookManager::GetRawFunc_Global_CreateEntityByName()
{
	static RawCreateEntityByNameFn s_CreateEntityByNameFn = nullptr;
	if (!s_CreateEntityByNameFn)
	{
		constexpr const char* SIG =  "\x55" "\x8B\xEC" "\xE8\x00\x00\x00\x00" "\xFF\x75\x08" "\x8B\xC8" "\x8B\x10" "\xFF\x00\x00" "\x85\xC0" "\x75\x13" "\xFF\x75\x08" "\x68\x00\x00\x00\x00" "\xFF\x00\x00\x00\x00\x00" "\x83\xC4\x08" "\x33\xC0" "\x5D" "\xC3";
		constexpr const char* MASK = "x" "xx" "x????" "xxx" "xx" "xx" "x??" "xx" "xx" "xxx" "x????" "x?????" "xxx" "xx" "x" "x";

		s_CreateEntityByNameFn = (RawCreateEntityByNameFn)SignatureScan("client", SIG, MASK);
		if (!s_CreateEntityByNameFn)
			throw bad_pointer("CreateEntityByName");
	}

	return s_CreateEntityByNameFn;
}

HookManager::RawCreateTFGlowObjectFn HookManager::GetRawFunc_Global_CreateTFGlowObject()
{
	static RawCreateTFGlowObjectFn s_CreateTFGlowObjectFn = nullptr;
	if (!s_CreateTFGlowObjectFn)
	{
		constexpr const char* SIG = 
			"\x55"					// push ebp
			"\x8B\xEC"				// mov ebp, esp
			"\x57"					// push edi
			"\x68\x00\x00\x00\x00"	// push <size_t>
			"\xE8\x00\x00\x00\x00"	// call ???? (malloc?)
			"\x8B\xF8"				// mov edi, eax
			"\x83\xC4\x04"			// add esp, 4
			"\x85\xFF"				// test edi, edi
			"\x74\x5F"				// jz short ???? (return nullptr)
			"\x56"					// push esi
			"\x8B\xCF"				// mov ecx, edi <pThis>
			"\xE8\x00\x00\x00\x00"	// call C_BaseEntity::C_BaseEntity
			"\xFF\x75\x0C"			// push [ebp+serialNum]
			"\xC7\x07";				// mov dword ptr [edi], ...
		constexpr const char* MASK = "xxxxx????x????xxxxxxxxxxxxx????xxxxx";

		s_CreateTFGlowObjectFn = (RawCreateTFGlowObjectFn)SignatureScan("client", SIG, MASK);
		if (!s_CreateTFGlowObjectFn)
			throw bad_pointer("_C_TFGlow_CreateObject");
	}

	return s_CreateTFGlowObjectFn;
}

HookManager::RawBaseEntityInitFn HookManager::GetRawFunc_C_BaseEntity_Init()
{
	static RawBaseEntityInitFn s_BaseEntityInitFn = nullptr;
	if (!s_BaseEntityInitFn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x8B\x45\x08\x56\xFF\x75\x0C\x8B\xF1\x8D\x4E\x04";
		constexpr const char* MASK = "xxxxxxxxxxxxxxx";

		s_BaseEntityInitFn = (RawBaseEntityInitFn)SignatureScan("client", SIG, MASK);
		if (!s_BaseEntityInitFn)
			throw bad_pointer("CBaseEntity::Init");
	}

	return s_BaseEntityInitFn;
}

HookManager::RawUTILComputeEntityFadeFn HookManager::GetRawFunc_Global_UTILComputeEntityFade()
{
	static RawUTILComputeEntityFadeFn s_UTILComputeEntityFadeFn = nullptr;
	if (!s_UTILComputeEntityFadeFn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x83\xEC\x40\x80\x3D";
		constexpr const char* MASK = "xxxxxxxx";

		s_UTILComputeEntityFadeFn = (RawUTILComputeEntityFadeFn)SignatureScan("client", SIG, MASK);
		if (!s_UTILComputeEntityFadeFn)
			throw bad_pointer("UTIL_ComputeEntityFade");
	}

	return s_UTILComputeEntityFadeFn;
}

HookManager::RawApplyEntityGlowEffectsFn HookManager::GetRawFunc_CGlowObjectManager_ApplyEntityGlowEffects()
{
	static RawApplyEntityGlowEffectsFn s_RawApplyEntityGlowEffectsFn = nullptr;
	if (!s_RawApplyEntityGlowEffectsFn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x83\xEC\x3C\x53\x56\x57\x8B\xD9\x8B\x0D";
		constexpr const char* MASK = "xxxxxxxxxxxxx";

		s_RawApplyEntityGlowEffectsFn = (RawApplyEntityGlowEffectsFn)SignatureScan("client", SIG, MASK);
		if (!s_RawApplyEntityGlowEffectsFn)
			throw bad_pointer("CGlowObjectManager::ApplyEntityGlowEffects");
	}

	return s_RawApplyEntityGlowEffectsFn;
}

HookManager::RawGetIconFn HookManager::GetRawFunc_CHudBaseDeathNotice_GetIcon()
{
	static RawGetIconFn s_RawGetIconFn = nullptr;
	if (!s_RawGetIconFn)
	{
		constexpr const char* SIG = "\x55\x8B\xEC\x81\xEC\x00\x00\x00\x00\x83\x7D\x0C\x00\x56";
		constexpr const char* MASK = "xxxxx????xxxxx";

		s_RawGetIconFn = (RawGetIconFn)SignatureScan("client", SIG, MASK);
		if (!s_RawGetIconFn)
			throw bad_pointer("CHudBaseDeathNotice_GetIcon");
	}

	return s_RawGetIconFn;
}

void HookManager::IngameStateChanged(bool inGame)
{
	if (inGame)
	{
		m_Hook_IGameEventManager2_FireEventClientSide.AttachHook(std::make_shared<IGameEventManager2_FireEventClientSide::Inner>(Interfaces::GetGameEventManager(), &IGameEventManager2::FireEventClientSide));
	}
	else
	{
		m_Hook_IGameEventManager2_FireEventClientSide.DetachHook();
	}
}

HookManager::HookManager()
{
	Assert(!s_HookManager);
	m_Panel.reset(new Panel());
	m_Hook_ICvar_ConsoleColorPrintf.AttachHook(std::make_shared<ICvar_ConsoleColorPrintf::Inner>(g_pCVar, &ICvar::ConsoleColorPrintf));
	m_Hook_ICvar_ConsoleDPrintf.AttachHook(std::make_shared<ICvar_ConsoleDPrintf::Inner>(g_pCVar, &ICvar::ConsoleDPrintf));
	m_Hook_ICvar_ConsolePrintf.AttachHook(std::make_shared<ICvar_ConsolePrintf::Inner>(g_pCVar, &ICvar::ConsolePrintf));

	m_Hook_IClientEngineTools_InToolMode.AttachHook(std::make_shared<IClientEngineTools_InToolMode::Inner>(Interfaces::GetClientEngineTools(), &IClientEngineTools::InToolMode));
	m_Hook_IClientEngineTools_IsThirdPersonCamera.AttachHook(std::make_shared<IClientEngineTools_IsThirdPersonCamera::Inner>(Interfaces::GetClientEngineTools(), &IClientEngineTools::IsThirdPersonCamera));
	m_Hook_IClientEngineTools_SetupEngineView.AttachHook(std::make_shared<IClientEngineTools_SetupEngineView::Inner>(Interfaces::GetClientEngineTools(), &IClientEngineTools::SetupEngineView));

	m_Hook_IVEngineClient_GetPlayerInfo.AttachHook(std::make_shared<IVEngineClient_GetPlayerInfo::Inner>(Interfaces::GetEngineClient(), &IVEngineClient::GetPlayerInfo));

	m_Hook_IPrediction_PostEntityPacketReceived.AttachHook(std::make_shared<IPrediction_PostEntityPacketReceived::Inner>(Interfaces::GetPrediction(), &IPrediction::PostEntityPacketReceived));

	m_Hook_C_HLTVCamera_SetCameraAngle.AttachHook(std::make_shared<C_HLTVCamera_SetCameraAngle::Inner>(Interfaces::GetHLTVCamera(), GetRawFunc_C_HLTVCamera_SetCameraAngle()));
	m_Hook_C_HLTVCamera_SetMode.AttachHook(std::make_shared<C_HLTVCamera_SetMode::Inner>(Interfaces::GetHLTVCamera(), GetRawFunc_C_HLTVCamera_SetMode()));
	m_Hook_C_HLTVCamera_SetPrimaryTarget.AttachHook(std::make_shared<C_HLTVCamera_SetPrimaryTarget::Inner>(Interfaces::GetHLTVCamera(), GetRawFunc_C_HLTVCamera_SetPrimaryTarget()));

	m_Hook_C_BaseEntity_Init.AttachHook(std::make_shared<C_BaseEntity_Init::Inner>(GetRawFunc_C_BaseEntity_Init()));

	m_Hook_CGlowObjectManager_ApplyEntityGlowEffects.AttachHook(std::make_shared<CGlowObjectManager_ApplyEntityGlowEffects::Inner>(GetRawFunc_CGlowObjectManager_ApplyEntityGlowEffects()));
	
	m_Hook_Global_GetLocalPlayerIndex.AttachHook(std::make_shared<Global_GetLocalPlayerIndex::Inner>(GetRawFunc_Global_GetLocalPlayerIndex()));
	m_Hook_Global_UTILComputeEntityFade.AttachHook(std::make_shared<Global_UTILComputeEntityFade::Inner>(GetRawFunc_Global_UTILComputeEntityFade()));
}