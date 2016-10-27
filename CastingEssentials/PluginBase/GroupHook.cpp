#include "GroupHook.h"
#include <PolyHook.h>

void* IGroupHook::GetOriginalRawFn(const std::shared_ptr<PLH::IHook>& hook)
{
	PLH::VFuncSwap* realHook = assert_cast<PLH::VFuncSwap*>(hook.get());
	return realHook->GetOriginal<void*>();
}

std::shared_ptr<PLH::IHook> IGroupHook::SetupHook(BYTE** instance, int vtableOffset, BYTE* detourFunc)
{
	PLH::VFuncSwap* newHook = new PLH::VFuncSwap();
	newHook->SetupHook(instance, vtableOffset, detourFunc);
	if (!newHook->Hook())
	{
		Assert(0);
		return nullptr;
	}

	return std::shared_ptr<PLH::IHook>(newHook);
}

bool IGroupHook::Hook(std::shared_ptr<PLH::IHook> hk)
{
	return hk->Hook();
}

void IGroupHook::Unhook(std::shared_ptr<PLH::IHook> hk)
{
	hk->UnHook();
}