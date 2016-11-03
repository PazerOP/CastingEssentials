#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <type_traits>
#include "IBaseHook.h"

// Badly-named macros smh
#undef IGNORE

namespace Hooking
{
	namespace Internal
	{
		template<class Type, class RetVal, class... Args> using MemberFnPtr_Const = RetVal(Type::*)(Args...) const;
		template<class Type, class RetVal, class... Args> using MemberFnPtr = RetVal(Type::*)(Args...);
		template<class Type, class RetVal, class... Args> using MemFnVaArgsPtr = RetVal(__cdecl Type::*)(Args..., ...);
		template<class Type, class RetVal, class... Args> using MemFnVaArgsPtr_Const = RetVal(__cdecl Type::*)(Args..., ...) const;

		template<class Type, class RetVal, class... Args> using LocalDetourFnPtr = RetVal(__fastcall*)(Type*, void*, Args...);
		template<class RetVal, class... Args> using GlobalDetourFnPtr = RetVal(*)(Args...);
		template<class Type, class RetVal, class... Args> using LocalFnPtr = RetVal(__thiscall*)(Type*, Args...);

		template<class RetVal, class... Args> using GlobalVaArgsFnPtr = RetVal(__cdecl*)(Args..., ...);
		template<class Type, class RetVal, class... Args> using LocalVaArgsFnPtr = RetVal(__cdecl*)(Type*, Args..., ...);

		template<class RetVal, class... Args> using FunctionalType = std::function<RetVal(Args...)>;

		
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

	enum class HookType
	{
		Global,
		Class,
		Virtual
	};

	class IGroupHook
	{
	public:
		virtual ~IGroupHook() { }

		virtual void SetState(HookAction action) = 0;

		virtual void InitHook() = 0;

		virtual HookType GetType() const = 0;

	protected:
		static std::atomic<uint64> s_LastHook;
		
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
		template<class F> static int VTableOffset(F func)
		{
			union
			{
				F p;
				intptr_t i;
			};

			p = func;

			return MFI_GetVTblOffset((void*)i);
		}

		std::recursive_mutex m_BaseHookMutex;
		std::shared_ptr<IBaseHook> m_BaseHook;

		template<class OutType, class InType> static OutType evil_cast(InType input)
		{
			static_assert(std::is_pointer<InType>::value, "InType must be a pointer type!");
			static_assert(std::is_pointer<OutType>::value, "OutType must be a pointer type!");
			static_assert(sizeof(InType) == sizeof(OutType), "InType and OutType are not the same size!");

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
}