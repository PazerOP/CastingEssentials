#include "Funcs.h"
#include "PluginBase/Exceptions.h"

#include "MinHook.h"

#include <sourcehook_impl.h>

#include <Windows.h>
#include <Psapi.h>
#include <convar.h>
#include <icvar.h>
#include <cdll_int.h>

SourceHook::Impl::CSourceHookImpl g_SourceHook;
SourceHook::ISourceHook *g_SHPtr = &g_SourceHook;
int g_PLID = 0;

SH_DECL_HOOK1_void_vafmt(ICvar, ConsoleColorPrintf, const FMTFUNCTION(3, 4), 0, const Color &);
SH_DECL_HOOK0_void_vafmt(ICvar, ConsolePrintf, const FMTFUNCTION(2, 3), 0);
SH_DECL_HOOK0_void_vafmt(ICvar, ConsoleDPrintf, const FMTFUNCTION(2, 3), 0);
#pragma region Test
//SH_DECL_HOOK2(IVEngineClient, GetPlayerInfo, SH_NOATTRIB, 0, bool, int, player_info_s*);

struct __SourceHook_FHCls_IVEngineClientGetPlayerInfo0
{
	static __SourceHook_FHCls_IVEngineClientGetPlayerInfo0 ms_Inst;
	static ::SourceHook::MemFuncInfo ms_MFI;
	static ::SourceHook::IHookManagerInfo *ms_HI;
	static ::SourceHook::ProtoInfo ms_Proto;
	static int HookManPubFunc(bool store, ::SourceHook::IHookManagerInfo *hi)
	{
		using namespace ::SourceHook;
		GetFuncInfo((static_cast<bool (IVEngineClient::*)(int, player_info_s*)> (&IVEngineClient::GetPlayerInfo)), ms_MFI);
		if (g_SHPtr->GetIfaceVersion() != 5)
			return 1;

		if (g_SHPtr->GetImplVersion() < 5)
			return 1;

		if (store)
			ms_HI = hi;

		if (hi)
		{
			MemFuncInfo mfi = { true, -1, 0, 0 };
			GetFuncInfo(&__SourceHook_FHCls_IVEngineClientGetPlayerInfo0::Func, mfi);
			hi->SetInfo(1, ms_MFI.vtbloffs, ms_MFI.vtblindex, &ms_Proto, reinterpret_cast<void**>(reinterpret_cast<char*>(&ms_Inst) + mfi.vtbloffs)[mfi.vtblindex]);
		}
		return 0;
	}
	typedef fastdelegate::FastDelegate<bool, int, player_info_s*> FD;

	struct IMyDelegate : ::SourceHook::ISHDelegate
	{
		virtual bool Call(int p1, player_info_s* p2) = 0;
	};

	struct CMyDelegateImpl : IMyDelegate
	{
		FD m_Deleg;
		CMyDelegateImpl(FD deleg) : m_Deleg(deleg) { }
		bool Call(int p1, player_info_s* p2) { return m_Deleg(p1, p2); }
		void DeleteThis() { delete this; }
		bool IsEqual(ISHDelegate *pOtherDeleg) { return m_Deleg == static_cast<CMyDelegateImpl*>(pOtherDeleg)->m_Deleg; }
	};

	virtual bool Func(int p1, player_info_s* p2)
	{
		using namespace ::SourceHook;
		void *ourvfnptr = reinterpret_cast<void*>(*reinterpret_cast<void***>(reinterpret_cast<char*>(this) + ms_MFI.vtbloffs) + ms_MFI.vtblindex);
		void *vfnptr_origentry;
		META_RES status = MRES_IGNORED;
		META_RES prev_res;
		META_RES cur_res;
		typedef ReferenceCarrier< bool >::type my_rettype;
		my_rettype orig_ret;
		my_rettype override_ret;
		my_rettype plugin_ret;
		IMyDelegate *iter;
		IHookContext *pContext = g_SHPtr->SetupHookLoop(ms_HI, ourvfnptr, reinterpret_cast<void*>(this), &vfnptr_origentry, &status, &prev_res, &cur_res, &orig_ret, &override_ret);
		prev_res = MRES_IGNORED;
		while ((iter = static_cast<IMyDelegate*>(pContext->GetNext())))
		{
			cur_res = MRES_IGNORED;
			plugin_ret = iter->Call(p1, p2);
			prev_res = cur_res;
			if (cur_res > status) status = cur_res;
			if (cur_res >= MRES_OVERRIDE) *reinterpret_cast<my_rettype*>(pContext->GetOverrideRetPtr()) = plugin_ret;
		}
		if (status != MRES_SUPERCEDE && pContext->ShouldCallOrig())
		{
			bool (EmptyClass::*mfp)(int, player_info_s*);
			reinterpret_cast<void**>(&mfp)[0] = vfnptr_origentry;
			;
			orig_ret = (reinterpret_cast<EmptyClass*>(this)->*mfp)(p1, p2);
		}
		else orig_ret = override_ret;
		prev_res = MRES_IGNORED;
		while ((iter = static_cast<IMyDelegate*>(pContext->GetNext())))
		{
			cur_res = MRES_IGNORED;
			plugin_ret = iter->Call(p1, p2);
			prev_res = cur_res;
			if (cur_res > status) status = cur_res;
			if (cur_res >= MRES_OVERRIDE) *reinterpret_cast<my_rettype*>(pContext->GetOverrideRetPtr()) = plugin_ret;
		}
		const my_rettype *retptr = reinterpret_cast<const my_rettype*>((status >= MRES_OVERRIDE) ? pContext->GetOverrideRetPtr() : pContext->GetOrigRetPtr());
		g_SHPtr->EndContext(pContext);
		return *retptr;
	}
};
__SourceHook_FHCls_IVEngineClientGetPlayerInfo0 __SourceHook_FHCls_IVEngineClientGetPlayerInfo0::ms_Inst;
::SourceHook::MemFuncInfo __SourceHook_FHCls_IVEngineClientGetPlayerInfo0::ms_MFI;
::SourceHook::IHookManagerInfo *__SourceHook_FHCls_IVEngineClientGetPlayerInfo0::ms_HI;
int __SourceHook_FHAddIVEngineClientGetPlayerInfo(void *iface, ::SourceHook::ISourceHook::AddHookMode mode, bool post, __SourceHook_FHCls_IVEngineClientGetPlayerInfo0::FD handler)
{
	using namespace ::SourceHook;
	MemFuncInfo mfi = { true, -1, 0, 0 };
	GetFuncInfo((static_cast<bool (IVEngineClient::*)(int, player_info_s*)>(&IVEngineClient::GetPlayerInfo)), mfi);
	if (mfi.thisptroffs < 0 || !mfi.isVirtual)
		return false;

	return g_SHPtr->AddHook(g_PLID, mode, iface, mfi.thisptroffs, __SourceHook_FHCls_IVEngineClientGetPlayerInfo0::HookManPubFunc, new __SourceHook_FHCls_IVEngineClientGetPlayerInfo0::CMyDelegateImpl(handler), post);
}
bool __SourceHook_FHRemoveIVEngineClientGetPlayerInfo(void *iface, bool post, __SourceHook_FHCls_IVEngineClientGetPlayerInfo0::FD handler)
{
	using namespace ::SourceHook;
	MemFuncInfo mfi =
	{
		true, -1, 0, 0
	};
	GetFuncInfo((static_cast<bool (IVEngineClient::*)(int, player_info_s*)>(&IVEngineClient::GetPlayerInfo)), mfi);
	__SourceHook_FHCls_IVEngineClientGetPlayerInfo0::CMyDelegateImpl tmp(handler);
	return g_SHPtr->RemoveHook(g_PLID, iface, mfi.thisptroffs, __SourceHook_FHCls_IVEngineClientGetPlayerInfo0::HookManPubFunc, &tmp, post);
}
const ::SourceHook::PassInfo __SourceHook_ParamInfos_IVEngineClientGetPlayerInfo0[] =
{
	{ 1, 0, 0 },
	{ sizeof(int), ::SourceHook::GetPassInfo< int >::type, ::SourceHook::GetPassInfo< int >::flags },
	{ sizeof(player_info_s*), ::SourceHook::GetPassInfo< player_info_s* >::type, ::SourceHook::GetPassInfo< player_info_s* >::flags }
};
const ::SourceHook::PassInfo::V2Info __SourceHook_ParamInfos2_IVEngineClientGetPlayerInfo0[] =
{
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 }
};
::SourceHook::ProtoInfo __SourceHook_FHCls_IVEngineClientGetPlayerInfo0::ms_Proto =
{
	2,
	{ sizeof(bool), ::SourceHook::GetPassInfo< bool >::type, ::SourceHook::GetPassInfo< bool >::flags },
	__SourceHook_ParamInfos_IVEngineClientGetPlayerInfo0,
	0,
	{ 0, 0, 0, 0 },
	__SourceHook_ParamInfos2_IVEngineClientGetPlayerInfo0
};
#pragma endregion

Funcs::SetModeFn Funcs::m_SetModeOriginal = nullptr;
int Funcs::m_SetModeLastHookRegistered = 0;
std::map<int, std::function<void(C_HLTVCamera *, int &)>> Funcs::m_SetModeHooks;

//int Funcs::m_SetModelLastHookRegistered = 0;
//std::map<int, std::function<void(C_BaseEntity *, const model_t *&)>> Funcs::m_SetModelHooks;

Funcs::SetPrimaryTargetFn Funcs::m_SetPrimaryTargetOriginal = nullptr;
int Funcs::m_SetPrimaryTargetLastHookRegistered = 0;
std::map<int, std::function<void(C_HLTVCamera *, int &)>> Funcs::m_SetPrimaryTargetHooks;


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

int Funcs::AddHook_C_HLTVCamera_SetMode(std::function<void(C_HLTVCamera *, int &)> hook)
{
	if (m_SetModeHooks.size() == 0)
	{
		AddDetour_C_HLTVCamera_SetMode(Detour_C_HLTVCamera_SetMode);
	}

	m_SetModeHooks[++m_SetModeLastHookRegistered] = hook;

	return m_SetModeLastHookRegistered;
}

int Funcs::AddHook_C_HLTVCamera_SetPrimaryTarget(std::function<void(C_HLTVCamera *, int &)> hook)
{
	if (m_SetPrimaryTargetHooks.size() == 0)
	{
		AddDetour_C_HLTVCamera_SetPrimaryTarget(Detour_C_HLTVCamera_SetPrimaryTarget);
	}

	m_SetPrimaryTargetHooks[++m_SetPrimaryTargetLastHookRegistered] = hook;

	return m_SetPrimaryTargetLastHookRegistered;
}

void Funcs::Detour_C_HLTVCamera_SetMode(C_HLTVCamera *instance, void *, int iMode)
{
	for (auto iterator : m_SetModeHooks)
	{
		iterator.second(instance, iMode);
	}

	Funcs::CallFunc_C_HLTVCamera_SetMode(instance, iMode);
}

void Funcs::Detour_C_HLTVCamera_SetPrimaryTarget(C_HLTVCamera *instance, void *, int nEntity)
{
	for (auto iterator : m_SetPrimaryTargetHooks)
		iterator.second(instance, nEntity);

	Funcs::CallFunc_C_HLTVCamera_SetPrimaryTarget(instance, nEntity);
}

void Funcs::CallFunc_C_HLTVCamera_SetMode(C_HLTVCamera *instance, int iMode)
{
	if (m_SetModeOriginal)
		m_SetModeOriginal(instance, iMode);
	else
		GetFunc_C_HLTVCamera_SetMode()(instance, iMode);
}

void Funcs::CallFunc_C_HLTVCamera_SetPrimaryTarget(C_HLTVCamera *instance, int nEntity)
{
	if (m_SetPrimaryTargetOriginal)
		m_SetPrimaryTargetOriginal(instance, nEntity);
	else
		GetFunc_C_HLTVCamera_SetPrimaryTarget()(instance, nEntity);
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

int Funcs::AddHook_IVEngineClient_GetPlayerInfo(IVEngineClient *instance, fastdelegate::FastDelegate<bool, int, player_info_s*> hook, bool post)
{
	return SH_ADD_HOOK(IVEngineClient, GetPlayerInfo, instance, hook, post);
}

bool Funcs::CallFunc_IVEngineClient_GetPlayerInfo(IVEngineClient *instance, int ent_num, player_info_s *pinfo)
{
	return SH_CALL(instance, &IVEngineClient::GetPlayerInfo)(ent_num, pinfo);
}