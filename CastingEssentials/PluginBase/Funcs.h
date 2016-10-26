#pragma once
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <type_traits>
//#include "PluginBase/EZHook.h"

class C_HLTVCamera;
class QAngle;
class ICvar;
class C_BaseEntity;
struct model_t;
class IVEngineClient;
struct player_info_s;

namespace PLH
{
	class IHook;
	class VFuncSwap;
}

class Funcs final
{
	enum Func
	{
		ICvar_ConsoleColorPrintf,
		ICvar_ConsoleDPrintf,
		ICvar_ConsolePrintf,

		IVEngineClient_GetPlayerInfo,

		C_HLTVCamera_SetCameraAngle,
		C_HLTVCamera_SetMode,
		C_HLTVCamera_SetPrimaryTarget,

		Count,
	};

	class IVirtualHook
	{
	public:
		virtual ~IVirtualHook() { }

		virtual std::recursive_mutex& GetHooksTableMutex() = 0;
		virtual void* GetHooksTable() = 0;
	};

	template<Func fType, bool vaArgs, class Type, class RetVal, class... Args>
	class VirtualHook final : public IVirtualHook
	{
	public:
		typedef VirtualHook<fType, vaArgs, Type, RetVal, Args...> SelfType;
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

		static const Func FunctionType = fType;

	private:
		std::recursive_mutex& GetHooksTableMutex() override { return m_HooksTableMutex; }
		std::recursive_mutex m_HooksTableMutex;

		void* GetHooksTable() override { return &m_HooksTable; }
		typedef std::map<unsigned int, Functional> HooksTableType;
		HooksTableType m_HooksTable;

		Type* m_Instance;

		// Same thing
		union
		{
			MemFnPtrType m_TargetFunction;
			MemVaArgsFnPtrType m_TargetVaArgsFnPtrType;
		};

		void* m_DetourFunc;

		// Same thing
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
		VirtualHook(Type* instance, RetVal(Type::*targetFunc)(Args..., ...));
		VirtualHook(Type* instance, RetVal(Type::*targetFunc)(Args..., ...) const) :
			VirtualHook(instance, reinterpret_cast<RetVal(Type::*)(Args..., ...)>(targetFunc)) { }

		int AddHook(const Functional& newHook);
		void RemoveHook(int hookID, const char* funcName);
		Functional GetOriginal() { return GetOriginalImpl(std::index_sequence_for<Args...>{}); }

	private:
		void InitHook();
	};

	static IVirtualHook* GetVHook(Func fn);

	typedef VirtualHook<ICvar_ConsoleColorPrintf, true, ICvar, void, const Color&, const char*> Hook_ICvar_ConsoleColorPrintf;

public:
	typedef void(__thiscall *SetCameraAngleFn)(C_HLTVCamera*, QAngle&);
	typedef void(__thiscall *SetModeFn)(C_HLTVCamera*, int);
	typedef void(__thiscall *SetPrimaryTargetFn)(C_HLTVCamera*, int);
	typedef void(__fastcall *SetModeDetourFn)(C_HLTVCamera*, void*, int);
	typedef void(__fastcall *SetPrimaryTargetDetourFn)(C_HLTVCamera*, void*, int);

	typedef std::function<void(const Color&, const char*)> ICvar_ConsoleColorPrintfFn;
	typedef std::function<void(const char*)> ICvar_ConsoleDPrintfFn;
	typedef std::function<void(const char*)> ICvar_ConsolePrintfFn;
	typedef std::function<bool(int, player_info_s*)> IVEngineClient_GetPlayerInfoFn;
	typedef std::function<void(int)> C_HLTVCamera_SetModeFn;
	typedef std::function<void(int)> C_HLTVCamera_SetPrimaryTargetFn;

	static SetCameraAngleFn GetFunc_C_HLTVCamera_SetCameraAngle();
	static SetModeFn GetFunc_C_HLTVCamera_SetMode();
	static SetPrimaryTargetFn GetFunc_C_HLTVCamera_SetPrimaryTarget();

	static int AddHook_ICvar_ConsoleDPrintf(ICvar* instance, const ICvar_ConsoleDPrintfFn& hook);
	static int AddHook_ICvar_ConsolePrintf(ICvar* instance, const ICvar_ConsolePrintfFn& hook);
	static int AddHook_IVEngineClient_GetPlayerInfo(IVEngineClient *instance, const IVEngineClient_GetPlayerInfoFn& hook);
	static int AddHook_C_HLTVCamera_SetMode(const C_HLTVCamera_SetModeFn& hook);
	static int AddHook_C_HLTVCamera_SetPrimaryTarget(const C_HLTVCamera_SetPrimaryTargetFn& hook);

	static void RemoveHook_C_HLTVCamera_SetMode(int hookID);
	static void RemoveHook_C_HLTVCamera_SetPrimaryTarget(int hookID);

	static bool RemoveHook(int hookID, const char* funcName);

	static bool Load();
	static bool Unload();

	static std::unique_ptr<Hook_ICvar_ConsoleColorPrintf> s_Hook_ICvar_ConsoleColorPrintf;

	static ICvar_ConsoleDPrintfFn Original_ICvar_ConsoleDPrintf();
	static ICvar_ConsolePrintfFn Original_ICvar_ConsolePrintf();
	static IVEngineClient_GetPlayerInfoFn Original_IVEngineClient_GetPlayerInfo();
	static C_HLTVCamera_SetModeFn Original_C_HLTVCamera_SetMode();
	static C_HLTVCamera_SetPrimaryTargetFn Original_C_HLTVCamera_SetPrimaryTarget();

private:
	Funcs() { }
	~Funcs() { }

	static void* GetOriginalRawFn(const std::shared_ptr<PLH::IHook>& hook);
	static std::shared_ptr<PLH::IHook> SetupHook(BYTE** instance, int vtableOffset, BYTE* detourFunc);

	static std::shared_ptr<PLH::IHook> s_BaseHooks[Func::Count];
	static std::recursive_mutex s_BaseHooksMutex;

	static int m_SetModeLastHookRegistered;
	static std::map<int, std::function<void(C_HLTVCamera *, int &)>> m_SetModeHooks;
	//static int m_SetModelLastHookRegistered;
	//static std::map<int, std::function<void(C_BaseEntity *, const model_t *&)>> m_SetModelHooks;
	static int m_SetPrimaryTargetLastHookRegistered;
	static std::map<int, std::function<void(C_HLTVCamera *, int &)>> m_SetPrimaryTargetHooks;

	static std::atomic<uint64> s_LastHook;
	static std::recursive_mutex s_HooksTableMutex;
	static std::map<unsigned int, std::shared_ptr<PLH::IHook>> s_HooksTable;

	static SetModeFn m_SetModeOriginal;
	static SetPrimaryTargetFn m_SetPrimaryTargetOriginal;

	static bool AddDetour(void *target, void *detour, void *&original);

	static bool AddDetour_C_HLTVCamera_SetMode(SetModeDetourFn detour);
	static bool AddDetour_C_HLTVCamera_SetPrimaryTarget(SetPrimaryTargetDetourFn detour);

	static void __fastcall Detour_C_HLTVCamera_SetMode(C_HLTVCamera *, void *, int);
	static void __fastcall Detour_C_HLTVCamera_SetPrimaryTarget(C_HLTVCamera *, void *, int);

	static bool RemoveDetour_C_HLTVCamera_SetMode();
	static bool RemoveDetour_C_HLTVCamera_SetPrimaryTarget();

	static bool RemoveDetour(void *target);

	static SetCameraAngleFn s_SetCameraAngleFn;
	static SetModeFn s_SetModeFn;
	static SetPrimaryTargetFn s_SetPrimaryTargetFn;

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

extern void* SignatureScan(const char* moduleName, const char* signature, const char* mask);

namespace SourceHook
{
	class ISourceHook;
}
extern SourceHook::ISourceHook* g_SHPtr;

template<Funcs::Func fType, bool vaArgs, class Type, class RetVal, class... Args>
inline void Funcs::VirtualHook<fType, vaArgs, Type, RetVal, Args...>::SetDefaultPatchFn()
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

template<Funcs::Func fType, bool vaArgs, class Type, class RetVal, class... Args>
inline Funcs::VirtualHook<fType, vaArgs, Type, RetVal, Args...>::VirtualHook(Type* instance, MemVaArgsFnPtrType targetFunc)
{
	//static_assert(vaArgs == false, "Must supply a pointer to a function WITH variable arguments");

	SetDefaultPatchFn();
	Assert(m_HookedPatchFunction);
	Assert(m_UnhookedPatchFunction);

	m_DetourFunc = DefaultDetourFn();
	Assert(m_DetourFunc);

	m_Instance = instance;
	Assert(m_Instance);

	m_TargetFunction = reinterpret_cast<MemFnPtrType>(targetFunc);
	Assert(m_TargetFunction);
}

template<Funcs::Func fType, bool vaArgs, class Type, class RetVal, class... Args>
inline int Funcs::VirtualHook<fType, vaArgs, Type, RetVal, Args...>::AddHook(const Functional& newHook)
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

template<Funcs::Func fType, bool vaArgs, class Type, class RetVal, class... Args>
inline void Funcs::VirtualHook<fType, vaArgs, Type, RetVal, Args...>::RemoveHook(int hookID, const char* funcName)
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

template<Funcs::Func fType, bool vaArgs, class Type, class RetVal, class... Args>
inline void Funcs::VirtualHook<fType, vaArgs, Type, RetVal, Args...>::InitHook()
{
	Assert(m_Instance);
	Assert(m_TargetFunction);

	auto baseHook = s_BaseHooks[fType];
	if (!baseHook)
	{
		std::lock_guard<std::recursive_mutex> lock(s_BaseHooksMutex);
		baseHook = s_BaseHooks[fType];
		if (!baseHook)
			s_BaseHooks[fType] = baseHook = SetupHook(*(BYTE***)m_Instance, VTableOffset(m_Instance, m_TargetFunction), (BYTE*)m_DetourFunc);
	}
}

template<Funcs::Func fType, bool vaArgs, class Type, class RetVal, class... Args>
template<std::size_t fmtParameter>
inline void* Funcs::VirtualHook<fType, vaArgs, Type, RetVal, Args...>::DefaultDetourFn()
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

			using testType = ArgType<const Color&>::type;
			static_assert(!std::is_reference<testType>::value, "wtf");
			static_assert(std::is_pointer<testType>::value, "wtf");

			using swallow = int[];
			(void)swallow{ 0, (void(va_arg(vaArgList, ArgType<Args>::type)), 0)... };

			// 8192 is the length used internally by CCvar
			char buffer[8192];
			vsnprintf_s(buffer, _TRUNCATE, *fmt, vaArgList);
			va_end(vaArgList);

			// Now run all the hooks
			{
				std::lock_guard<std::recursive_mutex> locker(Funcs::GetVHook(fType)->GetHooksTableMutex());
				for (auto& hook : *(HooksTableType*)Funcs::GetVHook(fType)->GetHooksTable())
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
			std::lock_guard<std::recursive_mutex> locker(Funcs::GetVHook(fType)->GetHooksTableMutex());
			for (auto hook : *(HooksTableType*)Funcs::GetVHook(fType)->GetHooksTable())
				hook.second(args...);
		};
		return detourFn;
	}
}

template<Funcs::Func fType, bool vaArgs, class Type, class RetVal, class... Args>
template<std::size_t... Is>
inline typename Funcs::VirtualHook<fType, vaArgs, Type, RetVal, Args...>::Functional Funcs::VirtualHook<fType, vaArgs, Type, RetVal, Args...>::GetOriginalImpl(std::index_sequence<Is...>)
{
	// Make sure we're initialized so we don't have any nasty race conditions
	InitHook();

	auto hook = s_BaseHooks[fType];
	if (hook)
	{
		void* originalFnPtr = GetOriginalRawFn(hook);

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
