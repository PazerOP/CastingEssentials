#include "IBaseHook.h"

#include <PolyHook.h>
#include <Windows.h>

using namespace Hooking;

class VFuncSwapHook final : public IBaseHook
{
public:
	VFuncSwapHook() = delete;
	VFuncSwapHook(void* instance, void* detourFunc, int vTableIndex);
	virtual ~VFuncSwapHook();

	bool Hook() override;
	bool Unhook() override;

	void* GetOriginalFunction() const override { return m_OriginalFunction; }

private:
	void* m_Instance;
	void* m_DetourFunc;
	int m_VTableIndex;

	void* m_OriginalFunction;
	bool m_IsHooked;
};

std::shared_ptr<IBaseHook> Hooking::CreateVFuncSwapHook(void* instance, void* detourFunc, int vTableIndex)
{
	return std::shared_ptr<IBaseHook>(new VFuncSwapHook(instance, detourFunc, vTableIndex));
}

VFuncSwapHook::VFuncSwapHook(void* instance, void* detourFunc, int vTableIndex)
{
	m_OriginalFunction = nullptr;
	m_IsHooked = false;

	m_Instance = instance;
	m_DetourFunc = detourFunc;
	m_VTableIndex = vTableIndex;
}

VFuncSwapHook::~VFuncSwapHook()
{
	if (m_IsHooked)
		Unhook();
}

bool VFuncSwapHook::Hook()
{
	if (m_IsHooked)
		return true;

	void** vtable = (*(void***)m_Instance);
	void** vfunc = &vtable[m_VTableIndex];

	DWORD old;
	if (!VirtualProtect(vfunc, sizeof(void*), PAGE_READWRITE, &old))
		return false;

	m_OriginalFunction = *vfunc;
	*vfunc = m_DetourFunc;

	if (!VirtualProtect(vfunc, sizeof(void*), old, &old))
		throw std::exception("Failed to re-protect memory!");

	m_IsHooked = true;
	return true;
}

bool VFuncSwapHook::Unhook()
{
	if (!m_IsHooked)
		return true;

	void** vtable = (*(void***)m_Instance);
	void** vfunc = &vtable[m_VTableIndex];

	DWORD old;
	if (!VirtualProtect(vfunc, sizeof(void*), PAGE_READWRITE, &old))
		return false;

	*vfunc = m_OriginalFunction;
	m_OriginalFunction = nullptr;

	if (!VirtualProtect(vfunc, sizeof(void*), old, &old))
		throw std::exception("Failed to re-protect memory!");

	m_IsHooked = false;
	return true;
}

class DetourHook : public IBaseHook
{
public:
	DetourHook(void* func, void* detourFunc)
	{
		m_Detour.reset(new PLH::Detour());
		m_Detour->SetupHook(func, detourFunc);
	}
	virtual ~DetourHook() = default;

	bool Hook() override { return m_Detour->Hook(); }
	bool Unhook() override { m_Detour->UnHook(); return true; }

	void* GetOriginalFunction() const override { return m_Detour->GetOriginal<void*>(); }

private:
	std::unique_ptr<PLH::Detour> m_Detour;
};

std::shared_ptr<IBaseHook> Hooking::CreateDetour(void* func, void* detourFunc)
{
	return std::shared_ptr<IBaseHook>(new DetourHook(func, detourFunc));
}