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
		// Detours a global function.
		Global,
		GlobalClass,
		Class,

		// Detours a specific instance of a class (replaces a function pointer in a duplicated vtable).
		Virtual,

		// Detours all instances of a class (replaces a function pointer in the global vtable).
		VirtualGlobal,
	};

	class IGroupHook
	{
	public:
		virtual ~IGroupHook() { }

		virtual void SetState(HookAction action) = 0;
		virtual bool RemoveHook(int hookID, const char* funcName) = 0;

		virtual void InitHook() = 0;

		virtual HookType GetType() const = 0;
		virtual int GetUniqueHookID() const = 0;

	protected:
		static std::atomic<uint64> s_LastHook;

		std::recursive_mutex m_BaseHookMutex;
		std::shared_ptr<IBaseHook> m_BaseHook;
	};
}