#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <type_traits>

// Badly-named macros smh
#undef IGNORE

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

	std::recursive_mutex m_BaseHookMutex;
	std::shared_ptr<PLH::IHook> m_BaseHook;

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