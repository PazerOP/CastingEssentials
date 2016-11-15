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
}