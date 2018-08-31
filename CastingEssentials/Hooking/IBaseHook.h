#pragma once
#include <memory>

namespace Hooking
{
	class IBaseHook
	{
	public:
		virtual ~IBaseHook() = default;

		virtual bool Hook() = 0;
		virtual bool Unhook() = 0;

		virtual void* GetOriginalFunction() const = 0;
	};

	extern std::shared_ptr<IBaseHook> CreateVTableSwapHook(void* instance, void* detourFunc, int vTableIndex);
	extern std::shared_ptr<IBaseHook> CreateVFuncSwapHook(void* instance, void* detourFunc, int vTableIndex);
	extern std::shared_ptr<IBaseHook> CreateDetour(void* func, void* detourFunc);

	namespace Internal
	{
		extern constexpr int MFI_GetVTblOffset(void* mfp);
	}
	template<class F> static int VTableOffset(F func)
	{
		static_assert(std::is_member_function_pointer_v<F>);

		union
		{
			F p;
			intptr_t i;
		};

		p = func;

		return Internal::MFI_GetVTblOffset((void*)i);
	}
}
