#pragma once
#include "PluginBase/HookManager.h"

template<HookFunc fn>
class Hook final
{
	using Functional = typename HookDefinitions::HookFuncType<fn>::Hook::Functional;

public:
	Hook(Functional&& fn, bool enable = false) :
		m_Fn(std::move(fn)), m_HookID(-1)
	{
		if (enable)
			Enable();
	}
	~Hook()
	{
		if (IsEnabled())
			Disable();
	}

	// No copying allowed
	Hook(const Hook<fn>& other) = delete;
	Hook<fn>& operator=(const Hook<fn>& other) = delete;

	bool SetEnabled(bool enabled)
	{
		if (enabled)
			return Enable();
		else
			return Disable();
	}
	bool Enable()
	{
		if (IsEnabled())
			return false;

		m_HookID = GetHooks()->AddHook<fn>(m_Fn);
		return true;
	}
	bool Disable()
	{
		if (!IsEnabled())
			return true;

		if (GetHooks()->RemoveHook<fn>(m_HookID, __FUNCTION__))
		{
			m_HookID = -1;
			return true;
		}
		else
			return false;
	}

	void SetState(Hooking::HookAction action)
	{
		Assert(IsEnabled());
		if (!IsEnabled())
			return;

		GetHooks()->SetState<fn>(action);
	}

	__forceinline auto GetOriginal() const { return GetHooks()->GetOriginal<fn>(); }
	__forceinline bool IsInHook() const { return GetHooks()->IsInHook<fn>(); }
	__forceinline bool IsEnabled() const { return m_HookID >= 0; }

private:
	Functional m_Fn;
	int m_HookID;
};