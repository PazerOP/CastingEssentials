#include "IGroupHook.h"
#include <PolyHook.hpp>

using namespace Hooking;

std::atomic<uint64> IGroupHook::s_LastHook;

#if 0
void* IGroupHook::GetOriginalRawFn(const std::shared_ptr<PLH::IHook>& hook)
{
	switch (hook->GetType())
	{
		case PLH::HookType::VFuncSwap:
			return assert_cast<PLH::VFuncSwap*>(hook.get())->GetOriginal<void*>();

		case PLH::HookType::X86Detour:
			return assert_cast<PLH::X86Detour*>(hook.get())->GetOriginal<void*>();
	}

	Assert(!"Invalid hook type!");
	return nullptr;
}

std::shared_ptr<PLH::IHook> IGroupHook::SetupVFuncHook(void** instance, int vtableOffset, void* detourFunc)
{
	if (vtableOffset < 0)
		return nullptr;

	PLH::VFuncSwap* newHook = new PLH::VFuncSwap();
	newHook->SetupHook((BYTE**)instance, vtableOffset, (BYTE*)detourFunc);

	if (!newHook->Hook())
	{
		Assert(0);
		delete newHook;
		return nullptr;
	}

	return std::shared_ptr<PLH::IHook>(newHook);
}

std::shared_ptr<PLH::IHook> IGroupHook::SetupDetour(BYTE* baseFunc, BYTE* detourFunc)
{
	PLH::Detour* newHook = new PLH::Detour();
	newHook->SetupHook(baseFunc, detourFunc);
	if (!newHook->Hook())
	{
		Assert(0);
		delete newHook;
		return nullptr;
	}

	return std::shared_ptr<PLH::IHook>(newHook);
}
#endif