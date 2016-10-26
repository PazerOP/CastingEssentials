#pragma once
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <utility>
#include <PolyHook.h>

typedef struct _HookHandle* HookHandle;

class EZHookBase
{
protected:
	template<class I, class F> static int VTableOffset(I* instance, F func)
	{
		union
		{
			F p;
			intptr_t i;
		};

		p = func;

		return MFI_GetVtblOffset((void*)i);
	}

private:
	static int VTableOffset(void* mfp);
};

template<class Type, class RetVal, class... Args>
class EZHook final : EZHookBase
{
	typedef EZHook<Type, RetVal, Args...> SelfType;
	typedef RetVal(__fastcall *DetourFuncType)(SelfType*, Type*, Args...);
	typedef RetVal(Type::*MemberFuncType)(Args...);
	typedef std::function<RetVal(Args...)> OriginalFn;

public:
	EZHook() { m_Instance = nullptr; m_MemberFunc = nullptr; }
	EZHook(Type* instance, MemberFuncType mFunc) { m_Instance = instance; m_MemberFunc = mFunc; }

	HookHandle Add(const std::function<RetVal(Args...)>& func);
	bool Remove(HookHandle handle);

	OriginalFn GetOriginal() { return GetOriginalImpl(std::index_sequence_for<Args...>{}); }

protected:
	virtual DetourFuncType GetDetourFn();

private:
	Type* m_Instance;
	MemberFuncType m_MemberFunc;

	template<std::size_t... Is> OriginalFn GetOriginalImpl(std::index_sequence<Is...>);

	std::recursive_mutex m_HooksTableMutex;
	std::map<std::shared_ptr<_HookHandle>, std::function<RetVal(Args...)>> m_HooksTable;

	std::recursive_mutex m_BaseHookMutex;
	std::shared_ptr<PLH::VFuncSwap> m_BaseHook;
};

template<class Type, class RetVal, class... Args>
inline HookHandle EZHook<Type, RetVal, Args...>::Add(const std::function<RetVal(Args...)>& newHook)
{
	// Alloc new HookHandle
	std::shared_ptr<_HookHandle> newHandle(malloc(1));

	if (!m_BaseHook)
	{
		// Make sure another thread can't beat us to the punch
		std::lock_guard<std::recursive_mutex> lock(m_BaseHookMutex);
		if (!m_BaseHook)
		{
			m_BaseHook = std::shared_ptr(new PLH::VFuncSwap());
			DetourFuncType detourFunc = GetDetourFn();
			m_BaseHook->SetupHook(*(BYTE***)m_Instance, VTableOffset(m_Instance, m_MemberFunc), (BYTE*)detourFunc);
			m_BaseHook->Hook();
		}
	}

	// Add new hook
	{
		std::lock_guard<std::recursive_mutex> lock(m_HooksTableMutex);
		m_HooksTable.insert(std::make_pair(newHandle, newHook));
	}

	return newHandle.get();
}

template<class Type, class RetVal, class... Args>
inline typename EZHook<Type, RetVal, Args...>::DetourFuncType EZHook<Type, RetVal, Args...>::GetDetourFn()
{
	RetVal(__fastcall *detourFunc)(SelfType*, Type*, Args...) = [](SelfType* pThis, Type* type, Args... args)
	{
		// Run all the hooks
		std::lock_guard<std::recursive_mutex> locker(pThis->m_HooksTableMutex);
		for (auto hook : pThis->m_HooksTable)
			hook.second(args...);
	};
	return detourFunc;
}

template<class Type, class RetVal, class ...Args>
template<std::size_t... Is>
inline typename EZHook<Type, RetVal, Args...>::OriginalFn EZHook<Type, RetVal, Args...>::GetOriginalImpl(std::index_sequence<Is...>)
{
	if (m_BaseHook)
	{
		std::lock_guard<std::recursive_mutex> lock(m_BaseHookMutex);
		std::shared_ptr<PLH::VFuncSwap> base = m_BaseHook;

		typedef RetVal(__fastcall *originalFnType)(Type*, Args...);

		auto patch = [](originalFnType func, Type* instance, Args... args) { func(instance, args...); };
		return std::bind(patch, base->GetOriginal<originalFnType>(), m_Instance, (std::_Ph<(int)(Is + 1)>{})...);
	}
	else
		return std::bind(m_MemberFunc, m_Instance, (std::_Ph<(int)(Is + 1)>{})...);
}
