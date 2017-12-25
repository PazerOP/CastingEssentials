#pragma once
#include "GroupClassHook.h"

namespace Hooking
{
	template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class OriginalFnType, class DetourFnType, class MemFnType, class FunctionalType, class Type, class RetVal, class... Args>
	class BaseGroupGlobalVirtualHook : public BaseGroupManualClassHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, FunctionalType, Type, RetVal, Args...>
	{
	public:
		typedef BaseGroupGlobalVirtualHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, MemFnType, FunctionalType, Type, RetVal, Args...> BaseGroupGlobalVirtualHookType;
		typedef BaseGroupGlobalVirtualHookType SelfType;
		typedef BaseGroupManualClassHookType BaseType;
		typedef MemFnType MemFnType;

		BaseGroupGlobalVirtualHook(Type* instance, MemFnType memFn, DetourFnType detour = nullptr) : BaseType(nullptr, detour)
		{
			m_Instance = instance;
			m_MemberFunction = memFn;
			Assert(m_MemberFunction);
		}

		virtual HookType GetType() const override { return HookType::VirtualGlobal; }
		virtual int GetUniqueHookID() const override { return (int)hookID; }

	protected:
		Type* m_Instance;
		MemFnType m_MemberFunction;

	private:
		void InitHook() override
		{
			Assert(GetType() == HookType::VirtualGlobal);

			if (!m_DetourFunction)
				m_DetourFunction = (DetourFnType)DefaultDetourFn();

			Assert(m_Instance);
			Assert(m_MemberFunction);
			Assert(m_DetourFunction);

			if (!m_BaseHook)
			{
				std::lock_guard<std::recursive_mutex> lock(m_BaseHookMutex);
				if (!m_BaseHook)
				{
					// Only actual difference between this and non-global virtual hook (CreateVFuncSwapHook vs CreateVTableSwapHook). SMH
					m_BaseHook = CreateVFuncSwapHook(m_Instance, m_DetourFunction, VTableOffset(m_MemberFunction));
					m_BaseHook->Hook();
				}
			}
		}

		BaseGroupGlobalVirtualHook() = delete;
		BaseGroupGlobalVirtualHook(const SelfType& other) = delete;
	};

	template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args> class GroupGlobalVirtualHook;

	// Non variable-arguments version
	template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
	class GroupGlobalVirtualHook<FuncEnumType, hookID, false, Type, RetVal, Args...> :
		public BaseGroupGlobalVirtualHook<FuncEnumType, hookID, false, Internal::LocalFnPtr<Type, RetVal, Args...>, Internal::LocalDetourFnPtr<Type, RetVal, Args...>, Internal::MemberFnPtr<Type, RetVal, Args...>, Internal::LocalFunctionalType<Type, RetVal, Args...>, Type, RetVal, Args...>
	{
	public:
		typedef BaseGroupGlobalVirtualHookType BaseType;

		GroupGlobalVirtualHook(Type* instance, MemFnType fn, DetourFnType detour = nullptr) : BaseType(instance, fn, detour) { }
		GroupGlobalVirtualHook(Type* instance, RetVal(Type::*fn)(Args...) const, DetourFnType detour = nullptr) :
			BaseType(instance, reinterpret_cast<MemFnType>(fn), detour)
		{ }

	private:
		GroupGlobalVirtualHook() = delete;
		GroupGlobalVirtualHook(const SelfType& other) = delete;

		DetourFnType DefaultDetourFn() override { return Internal::SharedLocalDetourFn<SelfType, Type, RetVal, Args...>(this); }
	};

#if 0
	// Variable arguments version
	template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
	class GroupGlobalVirtualHook<FuncEnumType, hookID, true, Type, RetVal, Args...> :
		public BaseGroupGlobalVirtualHook<FuncEnumType, hookID, true, Internal::LocalVaArgsFnPtr<Type, RetVal, Args...>, Internal::LocalVaArgsFnPtr<Type, RetVal, Args...>, Internal::MemFnVaArgsPtr<Type, RetVal, Args...>, Type, RetVal, Args...>
	{
	public:
		typedef GroupGlobalVirtualHook<FuncEnumType, hookID, true, Type, RetVal, Args...> SelfType;
		typedef BaseGroupGlobalVirtualHookType BaseType;

		GroupGlobalVirtualHook(Type* instance, MemFnType fn, DetourFnType detour = nullptr) : BaseType(instance, fn, detour) { }
		GroupGlobalVirtualHook(Type* instance, RetVal(Type::*fn)(Args..., ...) const, DetourFnType detour = nullptr) :
			SelfType(instance, reinterpret_cast<MemFnType>(fn), detour) { }

	private:
		GroupGlobalVirtualHook() = delete;
		GroupGlobalVirtualHook(const SelfType& other) = delete;

		DetourFnType DefaultDetourFn() override { return Internal::LocalVaArgsDetourFn<SelfType, Type, RetVal, Args...>(this); }
	};
#endif
}