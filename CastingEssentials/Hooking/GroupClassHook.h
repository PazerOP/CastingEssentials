#pragma once
#include "GroupGlobalHook.h"

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class OriginalFnType, class DetourFnType, class Type, class RetVal, class... Args>
class BaseGroupClassHook : public BaseGroupGlobalHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, RetVal, Args...>
{
public:
	typedef BaseGroupClassHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, Type, RetVal, Args...> SelfType;
	typedef SelfType BaseGroupClassHookType;
	typedef BaseGroupGlobalHookType BaseType;
	//typedef RetVal(__fastcall *PatchFnType)(OriginalFnType, Type*, Args...);
	typedef std::function<RetVal(OriginalFnType, Type*, Args...)> PatchFnType;

	BaseGroupClassHook(Type* instance, OriginalFnType fn, DetourFnType detour = nullptr) : BaseType(fn, detour)
	{
		m_Instance = instance;
		Assert(m_Instance);

		SetDefaultPatchFn();
		Assert(m_PatchFunction);
	}

	virtual Functional GetOriginal() override { return GetOriginalImpl(std::index_sequence_for<Args...>{}); }

protected:
	static SelfType* This() { return assert_cast<SelfType*>(BaseThis()); }

	BaseGroupClassHook(ConstructorParam1* instance, ConstructorParam2* detour) : BaseType((ConstructorParam1*)detour)
	{
		m_Instance = (Type*)instance;
		Assert(m_Instance);

		SetDefaultPatchFn();
		Assert(m_PatchFunction);
	}

	template<std::size_t fmtParameter = sizeof...(Args)-1> static DetourFnType VaArgsDetourFn()
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

	static DetourFnType StandardDetourFn()
	{
		RetVal(__fastcall *detourFn)(Type*, void*, Args...) = [](Type* pThis, void* edx, Args... args)
		{
			Assert(pThis == This()->m_Instance);
			//dummy = nullptr;
			//return This()->GetOriginal()(args...);
			return Stupid<RetVal>::InvokeHookFunctions(args...);
		};
		return detourFn;
	}

	template<std::size_t... Is> Functional GetOriginalImpl(std::index_sequence<Is...>);

	Type* m_Instance;
	PatchFnType m_PatchFunction;

	BaseGroupClassHook() = delete;
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
	DetourFnType DefaultDetourFn() override { return StandardDetourFn(); }
};

// Variable arguments version
template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
class GroupClassHook<FuncEnumType, hookID, true, Type, RetVal, Args...> :
	public BaseGroupClassHook<FuncEnumType, hookID, true, RetVal(__cdecl *)(Type*, Args..., ...), RetVal(__cdecl *)(Type*, Args..., ...), Type, RetVal, Args...>
{
public:
	typedef BaseGroupClassHookType BaseType;

private:
	DetourFnType DefaultDetourFn() override { return VaArgsDetourFn(); }
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
		OriginalFnType originalFnPtr;
		{
			std::lock_guard<std::recursive_mutex> lock(m_BaseHookMutex);
			originalFnPtr = reinterpret_cast<OriginalFnType>(GetOriginalRawFn(m_BaseHook));
		}

		//const auto originalFnTypeName = typeid(OriginalFnType).name();
		//const auto patchFnTypeName = typeid(PatchFnType).name();
		//const auto detourFnTypeName = typeid(DetourFnType).name();

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
