#pragma once
#include "IGroupHook.h"
#include "HookFunctionsInvoker.h"
#include "VariablePusher.h"
#include <stack>

namespace Hooking
{
	template <class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
	class GroupVirtualHook2 : public IGroupHook
	{
	public:
		static const FuncEnumType HOOK_ID = hookID;
		using SelfType = GroupVirtualHook2<FuncEnumType, hookID, Type, RetVal, Args...>;

		using Functional = Internal::FunctionalType<RetVal, Args...>;
		using OriginalFnPtr = Internal::LocalFnPtr<Type, RetVal, Args...>;
		using DetourFnPtr = Internal::LocalDetourFnPtr<Type, RetVal, Args...>;
		using MemFnType = Internal::MemberFnPtr<Type, RetVal, Args...>;

		GroupVirtualHook2(Type* instance, MemFnType fn)
		{
			Assert(!s_This);
			s_This = this;

			m_Instance = instance;
			Assert(m_Instance);

			Assert(fn);

			static_assert(std::is_same<decltype(&StandardDetourFn), DetourFnPtr>::value, "Programmer error smh");
			m_BaseHook = CreateVFuncSwapHook(m_Instance, &StandardDetourFn, VTableOffset(fn));
			m_BaseHook->Hook();
		}

		HookType GetType() const override { return HookType::Virtual; }
		void SetState(HookAction action) override { Assert(s_ResultStack); s_ResultStack->push(action); }

		void InitHook() override { Assert(0); }

		uint64 AddHook(const Functional& newHook)
		{
			const auto newIndex = ++s_LastHook;
			std::lock_guard<std::recursive_mutex> lock(m_HooksTableMutex);
			m_HooksTable.insert(std::make_pair(newIndex, newHook));
			return newIndex;
		}
		bool RemoveHook(uint64 hookID, const char* funcName)
		{
			std::lock_guard<std::recursive_mutex> lock(m_HooksTableMutex);

			auto found = m_HooksTable.find(hookID);
			if (found == m_HooksTable.end())
			{
				PluginWarning("Function %s called %s with invalid hook ID %i!\n", funcName, __FUNCSIG__, hookID);
				return false;
			}

			m_HooksTable.erase(found);
			return true;
		}
		
	private:
		static SelfType* This() { return assert_cast<SelfType*>(s_This); }
		static IGroupHook* s_This;
		std::shared_ptr<IBaseHook> m_BaseHook;

		std::map<uint64, Functional> m_HooksTable;
		std::recursive_mutex m_HooksTableMutex;

		static thread_local std::stack<HookAction>* s_ResultStack;

		Type* m_Instance;

		static RetVal __fastcall StandardDetourFn(Type* pThis, void* /* edx */, Args... args)
		{
			bool shouldCallOriginal;
			RetVal retVal;
			{
				std::stack<HookAction> currentStack;
				Internal::VariablePusher<std::stack<HookAction>> pusher(&s_ResultStack, &currentStack);
				std::lock_guard<std::recursive_mutex> locker(This()->m_HooksTableMutex);

				shouldCallOriginal = Internal::HookFunctionsInvoker<SelfType, Type, RetVal, Args...>::Stupid<RetVal>::InvokeHookFunctions(
					currentStack, This()->m_HooksTable, &retVal, This()->m_Instance, args...);
			}

			if (shouldCallOriginal)
				return reinterpret_cast<OriginalFnPtr>(This()->m_BaseHook->GetOriginalFunction())(pThis, args...);
			else
				return retVal;
		}

		static std::stack<HookAction>* INITIIAZLIE_LOL()
		{
			return nullptr;
		}
	};

	template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
	IGroupHook* GroupVirtualHook2<FuncEnumType, hookID, Type, RetVal, Args...>::s_This = nullptr;

	template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
	thread_local std::stack<HookAction>* GroupVirtualHook2<FuncEnumType, hookID, Type, RetVal, Args...>::s_ResultStack = INITIIAZLIE_LOL();
}