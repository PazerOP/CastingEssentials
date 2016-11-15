#include "IBaseHook.h"

#include <PolyHook.h>
#include <Windows.h>

#include <map>

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

class VTableSwapHook final : public IBaseHook
{
public:
	VTableSwapHook() = delete;
	VTableSwapHook(const VTableSwapHook& other) = delete;
	VTableSwapHook(void* instance, void* detourFunc, int vTableIndex)
	{
		// Find/create new VTableSwap hook
		{
			std::lock_guard<decltype(s_HooksTableMutex)> lock(s_HooksTableMutex);

			auto existing = s_HooksTable.find(instance);
			if (existing == s_HooksTable.end())
			{
				m_VTableSwapHook = std::make_shared<SharedHook>();
				s_HooksTable.insert(std::make_pair(instance, m_VTableSwapHook));
			}
			else
				m_VTableSwapHook = existing->second;
		}

		m_Instance = instance;
		m_DetourFn = detourFunc;
		m_Hooked = false;
		m_VTableIndex = vTableIndex;
		m_OriginalFn = nullptr;
	}
	~VTableSwapHook()
	{
		if (m_Hooked)
			Unhook();

		m_VTableSwapHook.reset();
		std::lock_guard<decltype(s_HooksTableMutex)> lock(s_HooksTableMutex);
		const long count = s_HooksTable.at(m_Instance).use_count();
		if (count <= 1)
			s_HooksTable.erase(m_Instance);
	}

	bool Hook() override
	{
		std::lock_guard<decltype(m_VTableSwapHook->m_Mutex)> lock(m_VTableSwapHook->m_Mutex);

		if (!m_VTableSwapHook->m_Hooked)
		{
			m_VTableSwapHook->m_Hook.SetupHook((BYTE*)m_Instance, m_VTableIndex, (BYTE*)m_DetourFn);
			const bool retVal = (m_VTableSwapHook->m_Hooked = m_VTableSwapHook->m_Hook.Hook());

			if (retVal)
				m_OriginalFn = m_VTableSwapHook->m_Hook.GetOriginal<void*>();

			return retVal;
		}
		else
		{
			m_OriginalFn = m_VTableSwapHook->m_Hook.HookAdditional<void*>(m_VTableIndex, (BYTE*)m_DetourFn);
			return true;
		}
	}
	bool Unhook() override
	{
		std::lock_guard<decltype(m_VTableSwapHook->m_Mutex)> lock(m_VTableSwapHook->m_Mutex);

		void* const original = m_VTableSwapHook->m_Hook.HookAdditional<void*>(m_VTableIndex, (BYTE*)m_OriginalFn);
		Assert(original == m_OriginalFn);
		return (original == m_OriginalFn);
	}

	void* GetOriginalFunction() const override
	{
		return m_OriginalFn;
	}

private:
	void* m_Instance;
	void* m_DetourFn;
	void* m_OriginalFn;

	bool m_Hooked;
	int m_VTableIndex;

	struct SharedHook
	{
		std::recursive_mutex m_Mutex;
		PLH::VTableSwap m_Hook;
		bool m_Hooked;
	};
	std::shared_ptr<SharedHook> m_VTableSwapHook;
	static std::recursive_mutex s_HooksTableMutex;
	static std::map<void*, std::shared_ptr<SharedHook>> s_HooksTable;
};

std::recursive_mutex VTableSwapHook::s_HooksTableMutex;
std::map<void*, std::shared_ptr<VTableSwapHook::SharedHook>> VTableSwapHook::s_HooksTable;

std::shared_ptr<IBaseHook> Hooking::CreateVTableSwapHook(void* instance, void* detourFunc, int vTableIndex)
{
	return std::shared_ptr<IBaseHook>(std::make_shared<VTableSwapHook>(instance, detourFunc, vTableIndex));
}