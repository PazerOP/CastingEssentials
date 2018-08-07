#pragma once
#include "PluginBase/HookDefinitions.h"

#include <memory>

class HookManager final : HookDefinitions
{
	template<HookFunc fn> friend class Hook;

public:
	HookManager();

	static bool Load();
	static bool Unload();

	template<HookFunc fn> __forceinline typename HookFuncType<fn>::Hook::OriginalFnType GetFunc() { return GetHook<fn>()->GetOriginal(); }
	template<HookFunc fn> __forceinline typename HookFuncType<fn>::Hook* GetHook() { return static_cast<HookFuncType<fn>::Hook*>(m_Hooks[(int)fn].get()); };
	template<HookFunc fn> static __forceinline typename HookFuncType<fn>::Raw GetRawFunc() { return (HookFuncType<fn>::Raw)s_RawFunctions[(int)fn]; }

	template<HookFunc fn> inline int AddHook(const typename HookFuncType<fn>::Hook::Functional& hook)
	{
		auto hkPtr = GetHook<fn>();
		if (!hkPtr)
			return 0;

		return hkPtr->AddHook(hook);
	}
	template<HookFunc fn> inline bool RemoveHook(int hookID, const char* funcName)
	{
		auto hkPtr = GetHook<fn>();
		if (!hkPtr)
			return false;

		return hkPtr->RemoveHook(hookID, funcName);
	}
	template<HookFunc fn> inline typename HookFuncType<fn>::Hook::Functional GetOriginal()
	{
		auto hkPtr = GetHook<fn>();
		if (!hkPtr)
			return nullptr;

		return hkPtr->GetOriginal();
	}
	template<HookFunc fn> inline void SetState(Hooking::HookAction state)
	{
		auto hkPtr = GetHook<fn>();
		if (!hkPtr)
			return;

		hkPtr->SetState(state);
	}
	template<HookFunc fn> inline bool IsInHook()
	{
		auto hkPtr = GetHook<fn>();
		if (!hkPtr)
			return false;

		return hkPtr->IsInHook();
	}

private:
	std::unique_ptr<Hooking::IGroupHook> m_Hooks[(int)HookFunc::Count];

	template<HookFunc fn, class... Args> void InitHook(Args&&... args);
	template<HookFunc fn> void InitGlobalHook();

	static void* s_RawFunctions[(int)HookFunc::Count];
	static void InitRawFunctionsList();
	template<HookFunc fn> static void FindFunc(const char* signature, const char* mask, int offset = 0, const char* module = "client");
	static void FindFunc_C_BasePlayer_GetLocalPlayer();

	void IngameStateChanged(bool inGame);
	class Panel;
	std::unique_ptr<Panel> m_Panel;
};

extern std::byte* SignatureScan(const char* moduleName, const char* signature, const char* mask, int offset = 0);
extern std::byte* SignatureScanMultiple(const char* moduleName, const char* signature, const char* mask,
	const std::function<bool(std::byte* found)>& testFunc, int offset = 0);
extern HookManager* GetHooks();