#pragma once
#include "GroupGlobalHook.h"

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class OriginalFnType, class DetourFnType, class Type, class RetVal, class... Args>
class BaseGroupClassHook : public BaseGroupGlobalHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, RetVal, Args...>
{
public:
	typedef BaseGroupClassHook<FuncEnumType, hookID, false, OriginalFnType, DetourFnType, Type, RetVal, Args...> SelfType;
	typedef SelfType BaseGroupClassHookType;
	typedef BaseGroupGlobalHookType BaseType;
	typedef OriginalFnType OriginalFnType;
	typedef RetVal(__fastcall *PatchFnType)(OriginalFnType, Type*, Args...);

	BaseGroupClassHook(Type* instance, OriginalFnType fn, DetourFnType detour = nullptr) : BaseType(fn, detour)
	{
		m_Instance = instance;
		Assert(m_Instance);

		SetDefaultPatchFn();
		Assert(m_PatchFunction);
	}

	virtual Functional GetOriginal() override { return GetOriginalImpl(std::index_sequence_for<Args...>{}); }

protected:
	BaseGroupClassHook(Type* instance, DetourFnType detour) : BaseType(detour)
	{
		m_Instance = instance;
		Assert(m_Instance);

		SetDefaultPatchFn();
		Assert(m_PatchFunction);
	}

	template<std::size_t fmtParameter = sizeof...(Args)-1> static void* VaArgsDetourFn()
	{
		DetourFnType detourFn = [](Type* pThis, Args... args, ...)
		{
			using FmtType = typename std::tuple_element<fmtParameter, std::tuple<Args...>>::type;
			static_assert(!vaArgs || std::is_same<FmtType, const char*>::value || std::is_same<FmtType, char*>::value, "Invalid format string type!");

			std::tuple<Args...> blah(args...);

			// Fuck you, type system
			const char** fmt = evil_cast<const char**>(&std::get<fmtParameter>(blah));

			va_list vaArgList;
			va_start(vaArgList, pThis);

			char** parameters[] = { (char**)(&(va_arg(vaArgList, ArgType<Args>::type)))... };

			// 8192 is the length used internally by CCvar
			char buffer[8192];
			vsnprintf_s(buffer, _TRUNCATE, *fmt, vaArgList);
			va_end(vaArgList);

			// Can't have variable arguments in std::function, overwrite the "format" parameter with
			// the fully parsed buffer
			*parameters[fmtParameter] = &buffer[0];

			// Now run all the hooks
			return Stupid<RetVal>::InvokeHookFunctions(args...);
		};
		return detourFn;
	}

	void SetDefaultPatchFn()
	{
		m_PatchFunction = [](OriginalFnType func, Type* instance, Args... args)
		{
			Assert(1);
			return func(instance, args...);
		};
	}

	static void* StandardDetourFn()
	{
		DetourFnType detourFn = [](Type* pThis, void* dummy, Args... args)
		{
			dummy = nullptr;
			return Stupid<RetVal>::InvokeHookFunctions(args...);
		};
		return detourFn;
	}

	template<std::size_t... Is> Functional GetOriginalImpl(std::index_sequence<Is...>);

	Type* m_Instance;
	PatchFnType m_PatchFunction;

#if !BASE_TYPE_COMPILER_BUG
	BaseGroupClassHook() = delete;
#else
	BaseGroupClassHook() { }
#endif
	BaseGroupClassHook(const SelfType& other) = delete;
};

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args> class GroupClassHook;

// Non variable-arguments version
template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
class GroupClassHook<FuncEnumType, hookID, false, Type, RetVal, Args...> :
	public BaseGroupClassHook<FuncEnumType, hookID, false, RetVal(__thiscall *)(Type*, Args...), RetVal(__fastcall*)(Type*, void*, Args...), Type, RetVal, Args...>
{
public:
	typedef BaseGroupClassHookType BaseType;

	GroupClassHook(Type* instance, OriginalFnType function, DetourFnType detour = nullptr) : BaseType(instance, function, detour) { }

private:
	void* DefaultDetourFn() override { return StandardDetourFn(); }
};

// Variable arguments version
template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
class GroupClassHook<FuncEnumType, hookID, true, Type, RetVal, Args...> :
	public BaseGroupClassHook<FuncEnumType, hookID, true, RetVal(__cdecl *)(Type*, Args..., ...), RetVal(__cdecl *)(Type*, Args..., ...), Type, RetVal, Args...>
{
public:
	typedef BaseGroupClassHookType BaseType;

private:
	void* DefaultDetourFn() override { return VaArgsDetourFn(); }
};

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class OriginalFnType, class DetourFnType, class Type, class RetVal, class... Args>
template<std::size_t ...Is>
inline typename BaseGroupClassHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, Type, RetVal, Args...>::Functional
BaseGroupClassHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, Type, RetVal, Args...>::GetOriginalImpl(std::index_sequence<Is...>)
{
	// Make sure we're initialized so we don't have any nasty race conditions
	InitHook();

	if (m_BaseHook)
	{
		//typedef RetVal(__thiscall* test)(Type*, Args...);
		OriginalFnType originalFnPtr = reinterpret_cast<OriginalFnType>(GetOriginalRawFn(m_BaseHook));
		return std::bind(m_PatchFunction, originalFnPtr, m_Instance, (std::_Ph<(int)(Is + 1)>{})...);
	}
	else
	{
		AssertMsg(0, "Should never get here... hook should be initialized so we don't have a potential race condition!");
		//if (vaArgs)
		//	return std::bind(m_UnhookedPatchVaArgsFunction, m_TargetVaArgsFnPtrType, m_Instance, (std::_Ph<(int)(Is + 1)>{})...);
		//else
		//	return std::bind(m_UnhookedPatchFunction, m_TargetFunction, m_Instance, (std::_Ph<(int)(Is + 1)>{})...);

		return nullptr;
	}
}
