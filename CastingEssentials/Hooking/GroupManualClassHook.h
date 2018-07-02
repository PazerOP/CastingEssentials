#pragma once
#include "Hooking/GroupGlobalHook.h"

namespace Hooking
{
	template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class OriginalFnType, class DetourFnType, class FunctionalType, class Type, class RetVal, class... Args> class BaseGroupManualClassHook :
		public BaseGroupGlobalHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, FunctionalType, RetVal, Type*, Args...>
	{
	public:
		using BaseGroupManualClassHookType = BaseGroupManualClassHook;
		using SelfType = BaseGroupManualClassHookType;
		using BaseType = BaseGroupGlobalHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, FunctionalType, RetVal, Type*, Args...>;
		using Functional = FunctionalType;

		virtual HookType GetType() const override { return HookType::GlobalClass; }
		virtual int GetUniqueHookID() const override { return (int)hookID; }

		BaseGroupManualClassHook(OriginalFnType fn, DetourFnType detour = nullptr) : BaseType(fn, detour) { }

		BaseGroupManualClassHook() = delete;
		BaseGroupManualClassHook(const SelfType& other) = delete;

		virtual Functional GetOriginal() override { return GetOriginalImpl(std::index_sequence_for<Args...>{}); }

	protected:
		static SelfType* This() { return assert_cast<SelfType*>(BaseThis()); }

		static Internal::LocalDetourFnPtr<Type, RetVal, Args...> SharedLocalDetourFn()
		{
			return [](Type* pThis, void*, Args... args)
			{
				Assert(This()->GetType() == HookType::GlobalClass || This()->GetType() == HookType::VirtualGlobal);
				return HookFunctionsInvoker<RetVal>::Invoke(pThis, args...);
			};
		}

		template<std::size_t... Is> Functional GetOriginalImpl(std::index_sequence<Is...>)
		{
			Assert(GetType() == HookType::GlobalClass || GetType() == HookType::VirtualGlobal);

			// Make sure we're initialized so we don't have any nasty race conditions
			InitHook();

			if (m_BaseHook)
			{
				OriginalFnType originalFnPtr = reinterpret_cast<OriginalFnType>(m_BaseHook->GetOriginalFunction());
				Assert(originalFnPtr);
				RetVal(*patch)(OriginalFnType, Type*, Args...) = [](OriginalFnType oFn, Type* instance, Args... args)
				{
					Assert(oFn);
					return oFn(instance, args...);
				};

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
		public BaseGroupManualClassHook<FuncEnumType, hookID, false, Internal::LocalFnPtr<Type, RetVal, Args...>, Internal::LocalDetourFnPtr<Type, RetVal, Args...>, Internal::LocalFunctionalType<Type, RetVal, Args...>, Type, RetVal, Args...>
	{
	public:
		using GroupManualClassHookType = GroupManualClassHook;
		using BaseType = BaseGroupManualClassHook<FuncEnumType, hookID, false, Internal::LocalFnPtr<Type, RetVal, Args...>, Internal::LocalDetourFnPtr<Type, RetVal, Args...>, Internal::LocalFunctionalType<Type, RetVal, Args...>, Type, RetVal, Args...>;
		using SelfType = GroupManualClassHookType;

		GroupManualClassHook(typename SelfType::OriginalFnType fn, typename SelfType::DetourFnType detour = nullptr) : BaseType(fn, detour) { }

		GroupManualClassHook() = delete;
		GroupManualClassHook(const SelfType& other) = delete;

	protected:
		typename SelfType::DetourFnType DefaultDetourFn() { return SharedLocalDetourFn(); }
	};

#if 0
	// Variable arguments version
	template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
	class GroupManualClassHook<FuncEnumType, hookID, true, Type, RetVal, Args...> :
		public BaseGroupManualClassHook<FuncEnumType, hookID, true, Internal::LocalVaArgsFnPtr<Type, RetVal, Args...>, Internal::LocalVaArgsFnPtr<Type, RetVal, Args...>, Type, RetVal, Args...>
	{
	public:
		typedef BaseGroupManualClassHookType BaseType;
	};
#endif
}