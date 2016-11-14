#pragma once
#include "Hooking/GroupGlobalHook.h"

namespace Hooking
{
	template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class OriginalFnType, class DetourFnType, class Type, class RetVal, class... Args> class BaseGroupManualClassHook :
		public BaseGroupGlobalHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, RetVal, Type*, Args...>
	{
	public:
		typedef BaseGroupManualClassHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, Type, RetVal, Args...> BaseGroupManualClassHookType;
		typedef BaseGroupManualClassHookType SelfType;
		typedef BaseGroupGlobalHookType BaseType;

		virtual HookType GetType() const override { return HookType::GlobalClass; }

		BaseGroupManualClassHook(OriginalFnType fn, DetourFnType detour = nullptr) : BaseType(fn, detour) { }

		BaseGroupManualClassHook() = delete;
		BaseGroupManualClassHook(const SelfType& other) = delete;

		virtual Functional GetOriginal() override { return GetOriginalImpl(std::index_sequence_for<Args...>{}); }

	protected:
		static SelfType* This() { return assert_cast<SelfType*>(BaseThis()); }

		template<std::size_t... Is> Functional GetOriginalImpl(std::index_sequence<Is...>)
		{
			Assert(GetType() == HookType::GlobalClass);

			// Make sure we're initialized so we don't have any nasty race conditions
			InitHook();

			if (m_BaseHook)
			{
				OriginalFnType originalFnPtr = reinterpret_cast<OriginalFnType>(m_BaseHook->GetOriginalFunction());
				RetVal(*patch)(OriginalFnType, Type*, Args...) = [](OriginalFnType oFn, Type* instance, Args... args) { return oFn(instance, args...); };

				std::function<RetVal(Type*, Args...)> fn = std::bind(patch, originalFnPtr, std::placeholders::_1, (std::_Ph<(int)(Is + 2)>{})...);

				return fn;
			}
			else
			{
				AssertMsg(0, "Should never get here... hook should be initialized so we don't have a potential race condition!");
				return nullptr;
			}
		}
	};

	template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args> class GroupManualClassHook;

	// Non-variable arguments version
	template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
	class GroupManualClassHook<FuncEnumType, hookID, false, Type, RetVal, Args...> :
		public BaseGroupManualClassHook<FuncEnumType, hookID, false, Internal::LocalFnPtr<Type, RetVal, Args...>, Internal::LocalDetourFnPtr<Type, RetVal, Args...>, Type, RetVal, Args...>
	{
	public:
		typedef GroupManualClassHook<FuncEnumType, hookID, false, Type, RetVal, Args...> GroupManualClassHookType;
		typedef GroupManualClassHookType SelfType;
		typedef BaseGroupManualClassHookType BaseType;

		GroupManualClassHook(OriginalFnType fn, DetourFnType detour = nullptr) : BaseType(fn, detour) { }

		GroupManualClassHook() = delete;
		GroupManualClassHook(const SelfType& other) = delete;

	protected:
		DetourFnType DefaultDetourFn()
		{
			DetourFnType fn = [](Type* pThis, void*, Args... args)
			{
				return Internal::HookFunctionsInvoker<BaseGroupHookType, RetVal, Type*, Args...>::Invoke(This(), pThis, args...);
			};
			return fn;
		}
	};

	// Variable arguments version
	template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
	class GroupManualClassHook<FuncEnumType, hookID, true, Type, RetVal, Args...> :
		public BaseGroupManualClassHook<FuncEnumType, hookID, true, Internal::LocalVaArgsFnPtr<Type, RetVal, Args...>, Internal::LocalVaArgsFnPtr<Type, RetVal, Args...>, Type, RetVal, Args...>
	{
	public:
		typedef BaseGroupManualClassHookType BaseType;
	};
}