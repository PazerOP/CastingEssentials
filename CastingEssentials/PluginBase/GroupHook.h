#pragma once
#include <atomic>
#include <map>
#include <mutex>
#include <stack>
#include <vector>

namespace PLH
{
	class IHook;
}

enum class HookAction
{
	// Call the original function and use its return value.
	// Continue calling all the remaining hooks.
	IGNORE,

	// Use our return value instead of the original function.
	// Don't call the original function.
	// Continue calling all the remaining hooks.
	SUPERCEDE,
};

class IGroupHook
{
public:
	virtual ~IGroupHook() { }

	virtual void SetState(HookAction action) = 0;

protected:
	static std::atomic<uint64> s_LastHook;

	static void* GetOriginalRawFn(const std::shared_ptr<PLH::IHook>& hook);
	static std::shared_ptr<PLH::IHook> SetupVFuncHook(BYTE** instance, int vTableOffset, BYTE* detourFunc);
	static std::shared_ptr<PLH::IHook> SetupDetour(BYTE* baseFunc, BYTE* detourFunc);

	template<typename T> struct ArgType
	{
		static_assert(!std::is_reference<T>::value, "error");
		static_assert(!std::is_const<T>::value, "error");
		typedef T type;
	};
	template<typename T> struct ArgType<T&>
	{
		typedef typename std::remove_reference<T>::type* type;
	};

	static int MFI_GetVTblOffset(void* mfp);
	template<class I, class F> static int VTableOffset(I* instance, F func)
	{
		union
		{
			F p;
			intptr_t i;
		};

		p = func;

		return MFI_GetVTblOffset((void*)i);
	}

	virtual void InitHook() = 0;

	template<class OutType, class InType> static OutType evil_cast(InType input)
	{
		static_assert(std::is_pointer<InType>::value, "InType must be a pointer type!");
		static_assert(std::is_pointer<OutType>::value, "OutType must be a pointer type!");

		// >:(
		union
		{
			InType goodIn;
			OutType evilOut;
		};

		goodIn = input;
		return evilOut;
	}
};

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
class GroupHook : public IGroupHook
{
	static_assert(std::is_enum<FuncEnumType>::value, "FuncEnumType must be an enum!");

public:
	typedef GroupHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...> SelfType;

	typedef std::function<RetVal(Args...)> Functional;
	typedef RetVal(__thiscall Type::*MemFnPtrType)(Args...);
	typedef RetVal(__cdecl Type::*MemVaArgsFnPtrType)(Args..., ...);

	typedef RetVal(__thiscall *OriginalFnType)(Type*, Args...);
	typedef RetVal(__cdecl *OriginalVaArgsFnType)(Type*, Args..., ...);
	typedef RetVal(__fastcall *UnhookedPatchFnType)(MemFnPtrType, Type*, Args...);
	typedef RetVal(__fastcall *UnhookedPatchVaArgsFnType)(MemVaArgsFnPtrType, Type*, Args...);
	typedef RetVal(__fastcall *HookedPatchFnType)(OriginalFnType, Type*, Args...);
	typedef RetVal(__fastcall *HookedPatchVaArgsFnType)(OriginalVaArgsFnType, Type*, Args...);
	typedef RetVal(__fastcall *DetourFnType)(Type*, void*, Args...);
	typedef RetVal(__cdecl *DetourVaArgsFnType)(Type*, Args..., ...);

	GroupHook(Type* instance, OriginalFnType fn);
	GroupHook(Type* instance, OriginalVaArgsFnType fn);

	int AddHook(const Functional& newHook);
	bool RemoveHook(int hookID, const char* funcName);
	Functional GetOriginal() { return GetOriginalImpl(std::index_sequence_for<Args...>{}); }

	void SetState(HookAction action) override;

protected:
	GroupHook(Type* instance) { m_Instance = instance; Assert(m_Instance); }

	std::recursive_mutex m_HooksTableMutex;
	std::map<uint64, Functional> m_HooksTable;
	static thread_local std::stack<HookAction> s_HookResults;

	Type* m_Instance;

	std::recursive_mutex m_BaseHookMutex;
	std::shared_ptr<PLH::IHook> m_BaseHook;

	// Same thing
	union
	{
		DetourFnType m_DetourFunction;
		DetourVaArgsFnType m_DetourVaArgsFunction;
	};
	union
	{
		UnhookedPatchFnType m_UnhookedPatchFunction;
		UnhookedPatchVaArgsFnType m_UnhookedPatchVaArgsFunction;
	};
	union
	{
		HookedPatchFnType m_HookedPatchFunction;
		HookedPatchVaArgsFnType m_HookedPatchVaArgsFunction;
	};

	void SetDefaultPatchFn();
	template<std::size_t fmtParameter = sizeof...(Args)-1> void* DefaultDetourFn();
	template<std::size_t... Is> Functional GetOriginalImpl(std::index_sequence<Is...>);

	// For constructors, const decorations don't matter
	typedef RetVal(__thiscall Type::*MemFnPtrTypeConst)(Args...) const;
	typedef RetVal(__cdecl Type::*MemVaArgsFnPtrTypeConst)(Args..., ...) const;

	static SelfType* This() { return static_cast<SelfType*>(s_This); }
	static IGroupHook* s_This;

	//template<class R> R InvokeHookFunctions(Args... args);

	template<bool _vaArgs, class _Type, class _RetVal, class... _Args> class Stupid
	{
	public:
		static _RetVal InvokeHookFunctions(Args... args);
	};
	template<bool _vaArgs, class _Type, class... _Args> class Stupid<_vaArgs, _Type, void, _Args...>
	{
	public:
		static void InvokeHookFunctions(Args... args)
		{
			const auto outerStartDepth = s_HookResults.size();
			bool callOriginal = true;
			auto& hooksTable = This()->m_HooksTable;
			for (auto hook : hooksTable)
			{
				const auto startDepth = s_HookResults.size();

				hook.second(args...);

				if (startDepth == s_HookResults.size())
					s_HookResults.push(HookAction::IGNORE);

				switch (s_HookResults.top())
				{
					case HookAction::IGNORE:	break;
					case HookAction::SUPERCEDE:
					{
						callOriginal = false;
						break;
					}
					default:	Assert(!"Invalid HookAction?");
				}

				s_HookResults.pop();
			}

			if (outerStartDepth != s_HookResults.size())
				Assert(!"Broken behavior: Someone called SetState too many times!");

			if (callOriginal)
				This()->GetOriginal()(args...);
		}
	};

private:
	void InitHook() override;

	union
	{
		OriginalFnType m_OriginalFunction;
		OriginalVaArgsFnType m_OriginalVaArgsFunction;
	};
};

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
IGroupHook* GroupHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::s_This = nullptr;

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
thread_local std::stack<HookAction> GroupHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::s_HookResults;

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
class GroupVirtualHook final : public GroupHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>
{
public:
	typedef GroupVirtualHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...> SelfType;

public:
	GroupVirtualHook(Type* instance, MemFnPtrType fn);
	GroupVirtualHook(Type* instance, MemFnPtrTypeConst fn) : GroupVirtualHook(instance, reinterpret_cast<MemFnPtrType>(fn)) { }

	GroupVirtualHook(Type* instance, MemVaArgsFnPtrType fn);
	GroupVirtualHook(Type* instance, MemVaArgsFnPtrTypeConst fn) : GroupVirtualHook(instance, reinterpret_cast<MemVaArgsFnPtrType>(fn)) { }
	//GroupVirtualHook(Type* instance, MemFnPtrType fn const) :
	//	GroupVirtualHook(instance, reinterpret_cast<RetVal(Type::*)(Args..., ...)>(targetFunc)) { }

private:
	void InitHook() override;

	static SelfType* This() { return static_cast<SelfType*>(s_This); }

	union
	{
		MemFnPtrType m_TargetFunction;
		MemVaArgsFnPtrType m_TargetVaArgsFnPtrType;
	};
};

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
inline void GroupHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::SetDefaultPatchFn()
{
	if (vaArgs)
	{
		m_HookedPatchVaArgsFunction = [](OriginalVaArgsFnType func, Type* instance, Args... args) { return func(instance, args...); };
		m_UnhookedPatchVaArgsFunction = [](MemVaArgsFnPtrType func, Type* instance, Args... args) { return (instance->*func)(args...); };
	}
	else
	{
		m_HookedPatchFunction = [](OriginalFnType func, Type* instance, Args... args) { return func(instance, args...); };
		m_UnhookedPatchFunction = [](MemFnPtrType func, Type* instance, Args... args) { return (instance->*func)(args...); };
	}
}

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
inline void GroupHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::InitHook()
{
	Assert(m_OriginalFunction);
	Assert(m_DetourFunction);

	if (!m_BaseHook)
	{
		std::lock_guard<std::recursive_mutex> lock(m_BaseHookMutex);
		if (!m_BaseHook)
			m_BaseHook = SetupDetour((BYTE*)m_OriginalFunction, (BYTE*)m_DetourFunction);
	}
}

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
inline GroupVirtualHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::GroupVirtualHook(Type * instance, MemFnPtrType fn) :
	GroupHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>(instance)
{
	Assert(!s_This);
	s_This = this;
	static_assert(vaArgs == false, "Must supply a pointer to a function WITHOUT variable arguments");

	SetDefaultPatchFn();
	Assert(m_HookedPatchFunction);
	Assert(m_UnhookedPatchFunction);

	m_DetourFunction = reinterpret_cast<DetourFnType>(DefaultDetourFn());
	Assert(m_DetourFunction);

	m_TargetFunction = reinterpret_cast<MemFnPtrType>(fn);
	Assert(m_TargetFunction);
}

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
inline GroupVirtualHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::GroupVirtualHook(Type* instance, MemVaArgsFnPtrType fn) :
	GroupHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>(instance)
{
	Assert(!s_This);
	s_This = this;
	static_assert(vaArgs == true, "Must supply a pointer to a function WITH variable arguments");

	SetDefaultPatchFn();
	Assert(m_HookedPatchFunction);
	Assert(m_UnhookedPatchFunction);

	m_DetourFunction = reinterpret_cast<DetourFnType>(DefaultDetourFn());
	Assert(m_DetourFunction);

	m_TargetFunction = reinterpret_cast<MemFnPtrType>(fn);
	Assert(m_TargetFunction);
}

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
inline GroupHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::GroupHook(Type* instance, OriginalFnType fn)
{
	Assert(!s_This);
	s_This = this;
	static_assert(vaArgs == false, "Must supply a pointer to a function WITHOUT variable arguments");

	SetDefaultPatchFn();
	Assert(m_HookedPatchFunction);
	Assert(m_UnhookedPatchFunction);

	m_DetourFunction = reinterpret_cast<DetourFnType>(DefaultDetourFn());
	Assert(m_DetourFunction);

	m_OriginalFunction = reinterpret_cast<OriginalFnType>(fn);
	Assert(m_OriginalFunction);

	m_Instance = instance;
	Assert(m_Instance);
}

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
inline GroupHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::GroupHook(Type* instance, OriginalVaArgsFnType fn)
{
	Assert(!s_This);
	s_This = this;
	static_assert(vaArgs == true, "Must supply a pointer to a function WITH variable arguments");

	SetDefaultPatchFn();
	Assert(m_HookedPatchFunction);
	Assert(m_UnhookedPatchFunction);

	m_DetourFunction = reinterpret_cast<DetourFnType>(DefaultDetourFn());
	Assert(m_DetourFunction);

	m_OriginalFunction = reinterpret_cast<OriginalFnType>(fn);
	Assert(m_OriginalFunction);

	m_Instance = instance;
	Assert(m_Instance);
}

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
inline int GroupHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::AddHook(const Functional& newHook)
{
	const auto newIndex = ++s_LastHook;

	// Make sure we're initialized
	InitHook();

	// Now add the new hook
	{
		std::lock_guard<std::recursive_mutex> lock(m_HooksTableMutex);
		m_HooksTable.insert(std::make_pair(newIndex, newHook));
	}

	return newIndex;
}

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
inline bool GroupHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::RemoveHook(int hookID, const char* funcName)
{
	std::lock_guard<std::recursive_mutex> lock(m_HooksTableMutex);

	auto found = m_HooksTable.find(hookID);
	if (found == m_HooksTable.end())
	{
		PluginWarning("Function %s called %s with invalid hook ID %i!\n", funcName, __FUNCSIG__, hookID);
		return false;
	}

	m_HooksTable.erase(found);

	return true;
}

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
inline void GroupHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::SetState(HookAction action)
{
	s_HookResults.push(action);
}

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
inline void GroupVirtualHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::InitHook()
{
	Assert(m_Instance);
	Assert(m_TargetFunction);
	Assert(m_DetourFunction);

	if (!m_BaseHook)
	{
		std::lock_guard<std::recursive_mutex> lock(m_BaseHookMutex);
		if (!m_BaseHook)
			m_BaseHook = SetupVFuncHook(*(BYTE***)m_Instance, VTableOffset(m_Instance, m_TargetFunction), (BYTE*)m_DetourFunction);
	}
}

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
template<std::size_t fmtParameter>
inline void* GroupHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::DefaultDetourFn()
{
	if (vaArgs)
	{
		DetourVaArgsFnType detourFn = [](Type* pThis, Args... args, ...)
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
			return Stupid<vaArgs, Type, RetVal, Args...>::InvokeHookFunctions(args...);
		};
		return detourFn;
	}
	else
	{
		DetourFnType detourFn = [](Type* pThis, void* dummy, Args... args)
		{
			return Stupid<vaArgs, Type, RetVal, Args...>::InvokeHookFunctions(args...);
		};
		return detourFn;
	}
}

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
template<std::size_t... Is>
inline typename GroupHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::Functional GroupHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::GetOriginalImpl(std::index_sequence<Is...>)
{
	// Make sure we're initialized so we don't have any nasty race conditions
	InitHook();

	if (m_BaseHook)
	{
		void* originalFnPtr = GetOriginalRawFn(m_BaseHook);

		if (vaArgs)
			return std::bind(m_HookedPatchVaArgsFunction, (OriginalVaArgsFnType)originalFnPtr, m_Instance, (std::_Ph<(int)(Is + 1)>{})...);
		else
			return std::bind(m_HookedPatchFunction, (OriginalFnType)originalFnPtr, m_Instance, (std::_Ph<(int)(Is + 1)>{})...);
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

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
template<bool _vaArgs, class _Type, class _RetVal, class... _Args>
inline _RetVal GroupHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::Stupid<_vaArgs, _Type, _RetVal, _Args...>::InvokeHookFunctions(Args... args)
{
	// Run all the hooks
	std::lock_guard<std::recursive_mutex> locker(This()->m_HooksTableMutex);

	const auto outerStartDepth = s_HookResults.size();
	int index = 0;
	int retValIndex = -1;
	_RetVal retVal;
	auto& hooksTable = This()->m_HooksTable;
	for (auto hook : hooksTable)
	{
		const auto startDepth = s_HookResults.size();
		const auto& temp = hook.second(args...);

		if (startDepth == s_HookResults.size())
			s_HookResults.push(HookAction::IGNORE);

		switch (s_HookResults.top())
		{
			case HookAction::IGNORE:	break;
			case HookAction::SUPERCEDE:
			{
				if (retValIndex != -1)
					Assert(!"Someone else already used HookAction::SUPERCEDE this hook!");
				
				retVal = temp;
				retValIndex = index;
				break;
			}
			default:	Assert(!"Invalid HookAction?");
		}

		s_HookResults.pop();

		index++;
	}

	if (retValIndex >= 0)
		return retVal;

	if (outerStartDepth != s_HookResults.size())
		Assert(!"Broken behavior: Someone called SetState too many times!");

	return This()->GetOriginal()(args...);
}