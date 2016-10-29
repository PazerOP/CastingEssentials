#pragma once
#include "GroupClassHook.h"

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class OriginalFnType, class DetourFnType, class MemFnType, class Type, class RetVal, class... Args>
class BaseGroupVirtualHook : public BaseGroupClassHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, Type, RetVal, Args...>
{
public:
	typedef BaseGroupVirtualHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, MemFnType, Type, RetVal, Args...> SelfType;
	typedef SelfType BaseGroupVirtualHookType;
	typedef BaseGroupClassHookType BaseType;
	typedef OriginalFnType OriginalFnType;
	typedef MemFnType MemFnType;

	BaseGroupVirtualHook(Type* instance, MemFnType fn, DetourFnType detour = nullptr) : BaseType(instance, detour)
	{
		m_MemberFunction = fn;
		Assert(m_MemberFunction);
	}

protected:
	MemFnType m_MemberFunction;

#if BASE_TYPE_COMPILER_BUG
	BaseGroupVirtualHook() { }
#endif

private:
	void InitHook() override
	{
		if (!m_DetourFunction)
			m_DetourFunction = (DetourFnType)DefaultDetourFn();

		Assert(m_Instance);
		Assert(m_MemberFunction);
		Assert(m_DetourFunction);

		if (!m_BaseHook)
		{
			std::lock_guard<std::recursive_mutex> lock(m_BaseHookMutex);
			if (!m_BaseHook)
				m_BaseHook = SetupVFuncHook(*(BYTE***)m_Instance, VTableOffset(m_Instance, m_MemberFunction), (BYTE*)m_DetourFunction);
		}
	}

#if !BASE_TYPE_COMPILER_BUG
	BaseGroupVirtualHook() = delete;
#endif
	BaseGroupVirtualHook(const SelfType& other) = delete;
};

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args> class GroupVirtualHook;

// Non variable-arguments version
template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
class GroupVirtualHook<FuncEnumType, hookID, false, Type, RetVal, Args...> :
	public BaseGroupVirtualHook<FuncEnumType, hookID, false, RetVal(__thiscall *)(Type*, Args...), RetVal(__fastcall*)(Type*, void*, Args...), RetVal(Type::*)(Args...), Type, RetVal, Args...>
{
public:
	typedef BaseGroupVirtualHookType BaseType;
	
	GroupVirtualHook(Type* instance, MemFnType fn, DetourFnType detour = nullptr) : BaseType(instance, fn, detour) { }
	GroupVirtualHook(Type* instance, RetVal(Type::*fn)(Args...) const, DetourFnType detour = nullptr) :
		BaseType(instance, reinterpret_cast<MemFnType>(fn), detour) { }

private:
	GroupVirtualHook() = delete;
	GroupVirtualHook(const SelfType& other) = delete;

	void* DefaultDetourFn() override { return StandardDetourFn(); }
};

// Variable arguments version
template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
class GroupVirtualHook<FuncEnumType, hookID, true, Type, RetVal, Args...> :
	public BaseGroupVirtualHook<FuncEnumType, hookID, true, RetVal(__cdecl *)(Type*, Args..., ...), RetVal(__cdecl *)(Type*, Args..., ...), RetVal(Type::*)(Args..., ...), Type, RetVal, Args...>
{
public:
	typedef GroupVirtualHook<FuncEnumType, hookID, true, Type, RetVal, Args...> SelfType;
	typedef BaseGroupVirtualHookType BaseType;

	// Compiler bug
#if BASE_TYPE_COMPILER_BUG
	GroupVirtualHook(Type* instance, MemFnType fn, DetourFnType detour = nullptr)
	{
		m_Instance = instance;
		Assert(m_Instance);

		m_MemberFunction = fn;
		Assert(m_MemberFunction);

		m_DetourFunction = detour;

		SetDefaultPatchFn();
		Assert(m_PatchFunction);
	}
	GroupVirtualHook(Type* instance, RetVal(Type::*fn)(Args..., ...) const, DetourFnType detour = nullptr) :
		SelfType(instance, reinterpret_cast<MemFnType>(fn), detour)	{ }
#else
	GroupVirtualHook(Type* instance, MemFnType fn, DetourFnType detour = nullptr) : BaseGroupVirtualHookType(instance, fn, detour) { }
	GroupVirtualHook(Type* instance, RetVal(Type::*fn)(Args..., ...) const, DetourFnType detour = nullptr) :
		SelfType(instance, reinterpret_cast<MemFnType>(fn), detour)	{ }
#endif

private:
	GroupVirtualHook() = delete;
	GroupVirtualHook(const SelfType& other) = delete;

	void* DefaultDetourFn() override { return VaArgsDetourFn(); }
};