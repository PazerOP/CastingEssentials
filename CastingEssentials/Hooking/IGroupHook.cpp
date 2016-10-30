#include "IGroupHook.h"
#include <PolyHook.h>

std::atomic<uint64> IGroupHook::s_LastHook;

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

std::shared_ptr<PLH::IHook> IGroupHook::SetupVFuncHook(BYTE** instance, int vtableOffset, BYTE* detourFunc)
{
	if (vtableOffset < 0)
		return nullptr;

	PLH::VFuncSwap* newHook = new PLH::VFuncSwap();
	newHook->SetupHook(instance, vtableOffset, detourFunc);

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

int IGroupHook::MFI_GetVTblOffset(void * mfp)
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
