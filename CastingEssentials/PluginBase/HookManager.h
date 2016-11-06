#pragma once
#include "Hooking/GroupGlobalHook.h"
#include "Hooking/GroupClassHook.h"
#include "Hooking/GroupVirtualHook.h"
#include "PluginBase/Modules.h"

#include <memory>

class C_HLTVCamera;
class QAngle;
class ICvar;
class C_BaseEntity;
struct model_t;
class IVEngineClient;
struct player_info_s;
typedef player_info_s player_info_t;
class IGameEventManager2;
class IGameEvent;

namespace PLH
{
	class IHook;
	class VFuncSwap;
}

class HookManager final
{
	enum class Func
	{
		ICvar_ConsoleColorPrintf,
		ICvar_ConsoleDPrintf,
		ICvar_ConsolePrintf,

		IVEngineClient_GetPlayerInfo,

		IGameEventManager2_FireEventClientSide,

		C_HLTVCamera_SetCameraAngle,
		C_HLTVCamera_SetMode,
		C_HLTVCamera_SetPrimaryTarget,

		Global_GetLocalPlayerIndex,

		Count,
	};

	template<Func fn, bool vaArgs, class Type, class RetVal, class... Args> using VirtualHook =
		Hooking::GroupVirtualHook<Func, fn, vaArgs, Type, RetVal, Args...>;
	template<Func fn, bool vaArgs, class Type, class RetVal, class... Args> using ClassHook =
		Hooking::GroupClassHook<Func, fn, vaArgs, Type, RetVal, Args...>;
	template<Func fn, bool vaArgs, class RetVal, class... Args> using GlobalHook =
		Hooking::GroupGlobalHook<Func, fn, vaArgs, RetVal, Args...>;

	typedef void(__thiscall *RawSetCameraAngleFn)(C_HLTVCamera*, const QAngle&);
	typedef void(__thiscall *RawSetModeFn)(C_HLTVCamera*, int);
	typedef void(__thiscall *RawSetPrimaryTargetFn)(C_HLTVCamera*, int);
	typedef int(*RawGetLocalPlayerIndexFn)();

	static RawSetCameraAngleFn GetRawFunc_C_HLTVCamera_SetCameraAngle();
	static RawSetModeFn GetRawFunc_C_HLTVCamera_SetMode();
	static RawSetPrimaryTargetFn GetRawFunc_C_HLTVCamera_SetPrimaryTarget();
	static RawGetLocalPlayerIndexFn GetRawFunc_Global_GetLocalPlayerIndex();

public:
	typedef VirtualHook<Func::ICvar_ConsoleColorPrintf, true, ICvar, void, const Color&, const char*> ICvar_ConsoleColorPrintf;
	typedef VirtualHook<Func::ICvar_ConsoleDPrintf, true, ICvar, void, const char*> ICvar_ConsoleDPrintf;
	typedef VirtualHook<Func::ICvar_ConsolePrintf, true, ICvar, void, const char*> ICvar_ConsolePrintf;

	typedef VirtualHook<Func::IVEngineClient_GetPlayerInfo, false, IVEngineClient, bool, int, player_info_t*> IVEngineClient_GetPlayerInfo;

	typedef VirtualHook<Func::IGameEventManager2_FireEventClientSide, false, IGameEventManager2, bool, IGameEvent*> IGameEventManager2_FireEventClientSide;

	typedef ClassHook<Func::C_HLTVCamera_SetCameraAngle, false, C_HLTVCamera, void, const QAngle&> C_HLTVCamera_SetCameraAngle;
	typedef ClassHook<Func::C_HLTVCamera_SetMode, false, C_HLTVCamera, void, int> C_HLTVCamera_SetMode;
	typedef ClassHook<Func::C_HLTVCamera_SetPrimaryTarget, false, C_HLTVCamera, void, int> C_HLTVCamera_SetPrimaryTarget;

	typedef GlobalHook<Func::Global_GetLocalPlayerIndex, false, int> Global_GetLocalPlayerIndex;

	bool Load();
	bool Unload();

	template<class Hook> typename Hook::Functional GetFunc() { static_assert(false, "Invalid hook type"); }
	template<> C_HLTVCamera_SetCameraAngle::Functional GetFunc<C_HLTVCamera_SetCameraAngle>();
	template<> C_HLTVCamera_SetMode::Functional GetFunc<C_HLTVCamera_SetMode>();
	template<> C_HLTVCamera_SetPrimaryTarget::Functional GetFunc<C_HLTVCamera_SetPrimaryTarget>();
	template<> Global_GetLocalPlayerIndex::Functional GetFunc<Global_GetLocalPlayerIndex>();

	template<class Hook> Hook* GetHook() { static_assert(false, "Invalid hook type"); }
	template<> ICvar_ConsoleColorPrintf* GetHook<ICvar_ConsoleColorPrintf>() { return m_Hook_ICvar_ConsoleColorPrintf.get(); }
	template<> ICvar_ConsoleDPrintf* GetHook<ICvar_ConsoleDPrintf>() { return m_Hook_ICvar_ConsoleDPrintf.get(); }
	template<> ICvar_ConsolePrintf* GetHook<ICvar_ConsolePrintf>() { return m_Hook_ICvar_ConsolePrintf.get(); }
	template<> IVEngineClient_GetPlayerInfo* GetHook<IVEngineClient_GetPlayerInfo>() { return m_Hook_IVEngineClient_GetPlayerInfo.get(); }
	template<> IGameEventManager2_FireEventClientSide* GetHook<IGameEventManager2_FireEventClientSide>() { return m_Hook_IGameEventManager2_FireEventClientSide.get(); }
	template<> C_HLTVCamera_SetCameraAngle* GetHook<C_HLTVCamera_SetCameraAngle>() { return m_Hook_C_HLTVCamera_SetCameraAngle.get(); }
	template<> C_HLTVCamera_SetMode* GetHook<C_HLTVCamera_SetMode>() { return m_Hook_C_HLTVCamera_SetMode.get(); }
	template<> C_HLTVCamera_SetPrimaryTarget* GetHook<C_HLTVCamera_SetPrimaryTarget>() { return m_Hook_C_HLTVCamera_SetPrimaryTarget.get(); }
	template<> Global_GetLocalPlayerIndex* GetHook<Global_GetLocalPlayerIndex>() { return m_Hook_Global_GetLocalPlayerIndex.get(); }

	template<class Hook> int AddHook(const typename Hook::Functional& hook)
	{
		auto hkPtr = GetHook<Hook>();
		if (!hkPtr)
			return 0;

		return hkPtr->AddHook(hook);
	}
	template<class Hook> bool RemoveHook(int hookID, const char* funcName)
	{
		auto hkPtr = GetHook<Hook>();
		if (!hkPtr)
			return false;

		return hkPtr->RemoveHook(hookID, funcName);
	}
	template<class Hook> typename Hook::Functional GetOriginal()
	{
		auto hkPtr = GetHook<Hook>();
		if (!hkPtr)
			return nullptr;

		return hkPtr->GetOriginal();
	}

private:
	std::unique_ptr<ICvar_ConsoleColorPrintf> m_Hook_ICvar_ConsoleColorPrintf;
	std::unique_ptr<ICvar_ConsoleDPrintf> m_Hook_ICvar_ConsoleDPrintf;
	std::unique_ptr<ICvar_ConsolePrintf> m_Hook_ICvar_ConsolePrintf;

	std::unique_ptr<IVEngineClient_GetPlayerInfo> m_Hook_IVEngineClient_GetPlayerInfo;

	std::unique_ptr<IGameEventManager2_FireEventClientSide> m_Hook_IGameEventManager2_FireEventClientSide;

	std::unique_ptr<C_HLTVCamera_SetCameraAngle> m_Hook_C_HLTVCamera_SetCameraAngle;
	std::unique_ptr<C_HLTVCamera_SetMode> m_Hook_C_HLTVCamera_SetMode;
	std::unique_ptr<C_HLTVCamera_SetPrimaryTarget> m_Hook_C_HLTVCamera_SetPrimaryTarget;

	std::unique_ptr<Global_GetLocalPlayerIndex> m_Hook_Global_GetLocalPlayerIndex;

	void IngameStateChanged(bool inGame);
	class Panel;
	std::unique_ptr<Panel> m_Panel;

	// Passthrough from Interfaces::GetHLTVCamera() so we don't have to #include "Interfaces.h" yet
	static C_HLTVCamera* GetHLTVCamera();
};

using ICvar_ConsoleColorPrintf = HookManager::ICvar_ConsoleColorPrintf;
using ICvar_ConsoleDPrintf = HookManager::ICvar_ConsoleDPrintf;
using ICvar_ConsolePrintf = HookManager::ICvar_ConsolePrintf;

using IVEngineClient_GetPlayerInfo = HookManager::IVEngineClient_GetPlayerInfo;

using IGameEventManager2_FireEventClientSide = HookManager::IGameEventManager2_FireEventClientSide;

using C_HLTVCamera_SetCameraAngle = HookManager::C_HLTVCamera_SetCameraAngle;
using C_HLTVCamera_SetMode = HookManager::C_HLTVCamera_SetMode;
using C_HLTVCamera_SetPrimaryTarget = HookManager::C_HLTVCamera_SetPrimaryTarget;

using Global_GetLocalPlayerIndex = HookManager::Global_GetLocalPlayerIndex;

extern void* SignatureScan(const char* moduleName, const char* signature, const char* mask);
extern HookManager* GetHooks();

template<> C_HLTVCamera_SetCameraAngle::Functional HookManager::GetFunc<C_HLTVCamera_SetCameraAngle>()
{
	return std::bind(
		[](RawSetCameraAngleFn func, C_HLTVCamera* pThis, const QAngle& ang) { func(pThis, ang); },
		GetRawFunc_C_HLTVCamera_SetCameraAngle(), GetHLTVCamera(), std::placeholders::_1);
}
template<> HookManager::C_HLTVCamera_SetMode::Functional HookManager::GetFunc<HookManager::C_HLTVCamera_SetMode>()
{
	return std::bind(
		[](RawSetModeFn func, C_HLTVCamera* pThis, int mode) { func(pThis, mode); },
		GetRawFunc_C_HLTVCamera_SetMode(), GetHLTVCamera(), std::placeholders::_1);
}
template<> C_HLTVCamera_SetPrimaryTarget::Functional HookManager::GetFunc<C_HLTVCamera_SetPrimaryTarget>()
{
	return std::bind(
		[](RawSetPrimaryTargetFn func, C_HLTVCamera* pThis, int target) { func(pThis, target); },
		GetRawFunc_C_HLTVCamera_SetPrimaryTarget(), GetHLTVCamera(), std::placeholders::_1);
}

template<> Global_GetLocalPlayerIndex::Functional HookManager::GetFunc<Global_GetLocalPlayerIndex>()
{
	return std::bind(GetRawFunc_Global_GetLocalPlayerIndex());
}