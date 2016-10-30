#pragma once
#include "BaseGroupHook.h"

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class OriginalFnType, class DetourFnType, class RetVal, class... Args>
class BaseGroupGlobalHook : public BaseGroupHook<FuncEnumType, hookID, RetVal, Args...>
{
	static_assert(!vaArgs || (sizeof...(Args) >= 1), "Must have at least 1 concrete argument defined for a variable-argument function");
public:
	typedef BaseGroupGlobalHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, RetVal, Args...> SelfType;
	typedef SelfType BaseGroupGlobalHookType;
	typedef OriginalFnType OriginalFnType;
	typedef DetourFnType DetourFnType;

	BaseGroupGlobalHook(OriginalFnType fn, DetourFnType detour = nullptr)
	{
		m_OriginalFunction = fn;
		Assert(m_OriginalFunction);

		m_DetourFunction = detour;
	}

	//virtual Functional GetOriginal() override { return GetOriginalImpl(std::index_sequence_for<Args...>{}); }

protected:
	static SelfType* This() { return assert_cast<SelfType*>(BaseThis()); }

	DetourFnType m_DetourFunction;

	virtual DetourFnType DefaultDetourFn() = 0;

	virtual void InitHook() override
	{
		if (!m_DetourFunction)
			m_DetourFunction = (DetourFnType)DefaultDetourFn();

		Assert(m_OriginalFunction);
		Assert(m_DetourFunction);

		if (!m_BaseHook)
		{
			std::lock_guard<std::recursive_mutex> lock(m_BaseHookMutex);
			if (!m_BaseHook)
				m_BaseHook = SetupDetour((BYTE*)m_OriginalFunction, (BYTE*)m_DetourFunction);
		}
	}

	template<std::size_t... Is> Functional GetOriginalImpl(std::index_sequence<Is...>);

	struct ConstructorParam1;
	struct ConstructorParam2;
	struct ConstructorParam3;
	BaseGroupGlobalHook(ConstructorParam1* detour)
	{
		m_OriginalFunction = nullptr;
		m_DetourFunction = (DetourFnType)detour;
	}

private:
	OriginalFnType m_OriginalFunction;

	BaseGroupGlobalHook() = delete;
	BaseGroupGlobalHook(const SelfType& other) = delete;
};

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class RetVal, class... Args> class GroupGlobalHook;

// Non variable-arguments version
template<class FuncEnumType, FuncEnumType hookID, class RetVal, class... Args>
class GroupGlobalHook<FuncEnumType, hookID, false, RetVal, Args...> final :
	public BaseGroupGlobalHook<FuncEnumType, hookID, false, RetVal(*)(Args...), RetVal(*)(Args...), RetVal, Args...>
{
public:
	typedef GroupGlobalHook<FuncEnumType, hookID, false, RetVal, Args...> SelfType;
	typedef SelfType GroupGlobalHookType;
	typedef BaseGroupGlobalHookType BaseType;

	GroupGlobalHook(OriginalFnType fn, DetourFnType detour = nullptr) : BaseType(fn, detour) { }

private:
	Functional GetOriginal() override { return GetOriginalImpl(std::index_sequence_for<Args...>{}); }

	static SelfType* This() { return assert_cast<SelfType*>(BaseThis()); }
	DetourFnType DefaultDetourFn() override
	{
		DetourFnType detourFn = [](Args... args)
		{
			return Stupid<RetVal>::InvokeHookFunctions(args...);
		};
		return detourFn;
	}

	GroupGlobalHook() = delete;
	GroupGlobalHook(const SelfType& other) = delete;
};

// Variable arguments version
template<class FuncEnumType, FuncEnumType hookID, class RetVal, class... Args>
class GroupGlobalHook<FuncEnumType, hookID, true, RetVal, Args...> final :
	public BaseGroupGlobalHook<FuncEnumType, hookID, true, RetVal(__cdecl *)(Args..., ...), RetVal(__cdecl *)(Args..., ...), RetVal, Args...>
{
public:
	typedef GroupGlobalHook<FuncEnumType, hookID, true, RetVal, Args...> SelfType;
	typedef SelfType GroupGlobalHookType;
	typedef BaseGroupGlobalHookType BaseType;

	GroupGlobalHook(OriginalFnType fn, DetourFnType detour = nullptr) : BaseType(fn, detour) { }

private:
	Functional GetOriginal() override { return GetOriginalImpl(std::index_sequence_for<Args...>{}); }

	static SelfType* This() { return assert_cast<SelfType*>(BaseThis()); }

	GroupGlobalHook() = delete;
	GroupGlobalHook(const SelfType& other) = delete;
};

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class OriginalFnType, class DetourFnType, class RetVal, class... Args>
template<std::size_t... Is>
inline typename BaseGroupGlobalHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, RetVal, Args...>::Functional BaseGroupGlobalHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, RetVal, Args...>::GetOriginalImpl(std::index_sequence<Is...>)
{
	// Make sure we're initialized so we don't have any nasty race conditions
	InitHook();

	if (m_BaseHook)
	{
		OriginalFnType originalFnPtr = (OriginalFnType)GetOriginalRawFn(m_BaseHook);
		return std::bind(originalFnPtr, (std::_Ph<(int)(Is + 1)>{})...);
	}
	else
	{
		AssertMsg(0, "Should never get here... hook should be initialized so we don't have a potential race condition!");
		return nullptr;
	}
}
