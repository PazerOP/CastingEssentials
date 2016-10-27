#pragma once
#include <atomic>
#include <map>
#include <mutex>
#include <vector>

namespace PLH
{
	class IHook;
}

class IGroupHook
{
public:
	virtual ~IGroupHook() { }

protected:
	static std::atomic<uint64> s_LastHook;

	static void* GetOriginalRawFn(const std::shared_ptr<PLH::IHook>& hook);
	static std::shared_ptr<PLH::IHook> SetupHook(BYTE** instance, int vTableOffset, BYTE* detourFunc);
	static bool Hook(std::shared_ptr<PLH::IHook> hk);
	static void Unhook(std::shared_ptr<PLH::IHook> hk);
};

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
class GroupHook : public IGroupHook
{
private:
	static std::recursive_mutex s_HooksTableMutex;
	static std::map<uint64, Functional> s_HooksTable;
};

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
class GroupVirtualHook final : public IGroupHook
{
	static_assert(std::is_enum<FuncEnumType>::value, "FuncEnumType must be an enum!");
public:
	typedef GroupVirtualHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...> SelfType;
	typedef std::function<RetVal(Args...)> Functional;
	typedef RetVal(__thiscall Type::*MemFnPtrType)(Args...);
	typedef RetVal(__cdecl Type::*MemVaArgsFnPtrType)(Args..., ...);

	typedef RetVal(__cdecl *OriginalVaArgsFnType)(Type*, Args..., ...);
	typedef RetVal(__thiscall *OriginalFnType)(Type*, Args...);
	typedef RetVal(__fastcall *UnhookedPatchFnType)(MemFnPtrType, Type*, Args...);
	typedef RetVal(__fastcall *UnhookedPatchVaArgsFnType)(MemVaArgsFnPtrType, Type*, Args...);
	typedef RetVal(__fastcall *HookedPatchFnType)(OriginalFnType, Type*, Args...);
	typedef RetVal(__fastcall *HookedPatchVaArgsFnType)(OriginalVaArgsFnType, Type*, Args...);
	typedef void(__fastcall *DetourFnType)(Type*, Args...);
	typedef void(__cdecl *DetourVaArgsFnType)(Type*, Args..., ...);

private:

	std::recursive_mutex m_BaseHookMutex;
	std::shared_ptr<PLH::IHook> m_BaseHook;

	Type* m_Instance;

	// Same thing
	union
	{
		MemFnPtrType m_TargetFunction;
		MemVaArgsFnPtrType m_TargetVaArgsFnPtrType;
	};
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

public:
	GroupVirtualHook(Type* instance, RetVal(Type::*targetFunc)(Args..., ...));
	GroupVirtualHook(Type* instance, RetVal(Type::*targetFunc)(Args..., ...) const) :
		GroupVirtualHook(instance, reinterpret_cast<RetVal(Type::*)(Args..., ...)>(targetFunc)) { }

	int AddHook(const Functional& newHook);
	bool RemoveHook(int hookID, const char* funcName);
	Functional GetOriginal() { return GetOriginalImpl(std::index_sequence_for<Args...>{}); }

private:
	void InitHook();

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
};

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
inline void GroupVirtualHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::SetDefaultPatchFn()
{
	if (vaArgs)
	{
		m_HookedPatchVaArgsFunction = [](OriginalVaArgsFnType func, Type* instance, Args... args) { func(instance, args...); };
		m_UnhookedPatchVaArgsFunction = [](MemVaArgsFnPtrType func, Type* instance, Args... args) { (instance->*func)(args...); };
	}
	else
	{
		m_HookedPatchFunction = [](OriginalFnType func, Type* instance, Args... args) { func(instance, args...); };
		m_UnhookedPatchFunction = [](MemFnPtrType func, Type* instance, Args... args) { (instance->*func)(args...); };
	}
}

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
inline GroupVirtualHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::GroupVirtualHook(Type* instance, MemVaArgsFnPtrType targetFunc)
{
	//static_assert(vaArgs == false, "Must supply a pointer to a function WITH variable arguments");

	SetDefaultPatchFn();
	Assert(m_HookedPatchFunction);
	Assert(m_UnhookedPatchFunction);

	m_DetourFunction = reinterpret_cast<DetourFnType>(DefaultDetourFn());
	Assert(m_DetourFunction);

	m_Instance = instance;
	Assert(m_Instance);

	m_TargetFunction = reinterpret_cast<MemFnPtrType>(targetFunc);
	Assert(m_TargetFunction);
}

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
inline int GroupVirtualHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::AddHook(const Functional& newHook)
{
	const auto newIndex = ++s_LastHook;

	// Make sure we're initialized
	InitHook();

	// Now add the new hook
	{
		std::lock_guard<std::recursive_mutex> lock(s_HooksTableMutex);
		s_HooksTable.insert(std::make_pair(newIndex, newHook));
	}

	return newIndex;
}

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
inline bool GroupVirtualHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::RemoveHook(int hookID, const char* funcName)
{
	std::lock_guard<std::recursive_mutex> lock(s_HooksTableMutex);

	auto found = s_HooksTable.find(hookID);
	if (found == s_HooksTable.end())
	{
		PluginWarning("Function %s called %s with invalid hook ID %i!\n", funcName, __FUNCSIG__, hookID);
		return false;
	}

	s_HooksTable.erase(found);

	return true;
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
		{
			m_BaseHook = SetupHook(*(BYTE***)m_Instance, VTableOffset(m_Instance, m_TargetFunction), (BYTE*)m_DetourFunction);
			Hook(m_BaseHook);
		}
	}
}

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
inline int GroupVirtualHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::MFI_GetVTblOffset(void * mfp)
{
	// Code stolen from SourceHook
	static_assert(_MSC_VER == 1900, "Only verified on VS2015!");

	unsigned char *addr = (unsigned char*)mfp;
	if (*addr == 0xE9)		// Jmp
	{
		// May or may not be!
		// Check where it'd jump
		addr += 5 /*size of the instruction*/ + *(unsigned long*)(addr + 1);
	}

	// Check whether it's a virtual function call
	// They look like this:
	// 004125A0 8B 01            mov         eax,dword ptr [ecx] 
	// 004125A2 FF 60 04         jmp         dword ptr [eax+4]
	//		==OR==
	// 00411B80 8B 01            mov         eax,dword ptr [ecx] 
	// 00411B82 FF A0 18 03 00 00 jmp         dword ptr [eax+318h]

	// However, for vararg functions, they look like this:
	// 0048F0B0 8B 44 24 04      mov         eax,dword ptr [esp+4]
	// 0048F0B4 8B 00            mov         eax,dword ptr [eax]
	// 0048F0B6 FF 60 08         jmp         dword ptr [eax+8]
	//		==OR==
	// 0048F0B0 8B 44 24 04      mov         eax,dword ptr [esp+4]
	// 0048F0B4 8B 00            mov         eax,dword ptr [eax]
	// 00411B82 FF A0 18 03 00 00 jmp         dword ptr [eax+318h]

	// With varargs, the this pointer is passed as if it was the first argument

	bool ok = false;
	if (addr[0] == 0x8B && addr[1] == 0x44 && addr[2] == 0x24 && addr[3] == 0x04 &&
		addr[4] == 0x8B && addr[5] == 0x00)
	{
		addr += 6;
		ok = true;
	}
	else if (addr[0] == 0x8B && addr[1] == 0x01)
	{
		addr += 2;
		ok = true;
	}
	if (!ok)
		return -1;

	if (*addr++ == 0xFF)
	{
		if (*addr == 0x60)
		{
			return *++addr / 4;
		}
		else if (*addr == 0xA0)
		{
			return *((unsigned int*)++addr) / 4;
		}
		else if (*addr == 0x20)
			return 0;
		else
			return -1;
	}
	return -1;
}

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
template<std::size_t fmtParameter>
inline void* GroupVirtualHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::DefaultDetourFn()
{
	if (vaArgs)
	{
		DetourVaArgsFnType detourFn = [](Type* pThis, Args... args, ...)
		{
			using FmtType = typename std::tuple_element<fmtParameter, std::tuple<Args...>>::type;
			static_assert(std::is_same<FmtType, const char*>::value || std::is_same<FmtType, char*>::value, "Invalid format string type!");

			std::tuple<Args...> blah(args...);
			const char** fmt = (&std::get<fmtParameter>(blah));

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
			{
				std::lock_guard<std::recursive_mutex> locker(s_HooksTableMutex);
				for (auto hook : s_HooksTable)
					hook.second(args...);
			}
		};
		return detourFn;
	}
	else
	{
		DetourFnType detourFn = [](Type* pThis, Args... args)
		{
			// Run all the hooks
			std::lock_guard<std::recursive_mutex> locker(s_HooksTableMutex);
			for (auto hook : s_HooksTable)
				hook.second(args...);
		};
		return detourFn;
	}
}

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
template<std::size_t... Is>
inline typename GroupVirtualHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::Functional GroupVirtualHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::GetOriginalImpl(std::index_sequence<Is...>)
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
		if (vaArgs)
			return std::bind(m_UnhookedPatchVaArgsFunction, m_TargetVaArgsFnPtrType, m_Instance, (std::_Ph<(int)(Is + 1)>{})...);
		else
			return std::bind(m_UnhookedPatchFunction, m_TargetFunction, m_Instance, (std::_Ph<(int)(Is + 1)>{})...);
	}
}

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
std::recursive_mutex GroupVirtualHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::s_HooksTableMutex;

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
std::map<uint64, typename GroupVirtualHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::Functional>
GroupVirtualHook<FuncEnumType, hookID, vaArgs, Type, RetVal, Args...>::s_HooksTable;