#pragma once
#include "IGroupHook.h"
#include "TemplateFunctions.h"

#include <map>
#include <shared_mutex>
#include <stack>

namespace Hooking
{
	template<class FuncEnumType, FuncEnumType hookID, class FunctionalType, class RetVal, class... Args>
	class BaseGroupHook : public IGroupHook
	{
		static_assert(std::is_enum<FuncEnumType>::value, "FuncEnumType must be an enum!");
		static constexpr FuncEnumType Dumb() { return hookID; }
	public:
		typedef FuncEnumType FuncEnumType;
		static constexpr FuncEnumType HOOK_ID = Dumb();
		typedef RetVal RetVal;

		using Functional = FunctionalType;
		typedef BaseGroupHook<FuncEnumType, hookID, FunctionalType, RetVal, Args...> SelfType;
		typedef SelfType BaseGroupHookType;

		virtual void SetState(HookAction action) override;
		virtual bool IsInHook() const override { return s_HookResults; }

		virtual int AddHook(const Functional& newHook);
		virtual bool RemoveHook(int hookID, const char* funcName) override;
		virtual Functional GetOriginal() = 0;

	protected:
		BaseGroupHook();
		BaseGroupHook(const SelfType& other) = delete;
		virtual ~BaseGroupHook();

		SelfType& operator=(const SelfType& other) = delete;

		std::shared_mutex m_HooksTableMutex;		// Mutex for m_HooksTable
		std::map<uint64, Functional> m_HooksTable;	// Map of hook indices to std::function callbacks
		static thread_local std::stack<HookAction>* s_HookResults;

		static SelfType* This() { return assert_cast<SelfType*>(BaseThis()); }
		static IGroupHook* BaseThis() { return s_This; }

		template<class _RetVal> struct HookFunctionsInvoker { static _RetVal Invoke(Args... args); };
		template<> struct HookFunctionsInvoker<void>
		{
			static void Invoke(Args... args)
			{
				std::shared_lock<std::shared_mutex> lock(This()->m_HooksTableMutex);

				std::stack<HookAction> newHookResults;
				auto hookResultsPusher = CreateVariablePusher(s_HookResults, &newHookResults);

				const auto outerStartDepth = newHookResults.size();
				bool callOriginal = true;
				for (auto& currentHook : This()->m_HooksTable)
				{
					const auto startDepth = newHookResults.size();

					currentHook.second(args...);

					if (startDepth == newHookResults.size())
						newHookResults.push(HookAction::IGNORE);

					Assert(newHookResults.size());
					switch (newHookResults.top())
					{
						case HookAction::IGNORE:	break;
						case HookAction::SUPERCEDE:
						{
							callOriginal = false;
							break;
						}
						default:	Assert(!"Invalid HookAction?");
					}

					newHookResults.pop();
				}

				if (outerStartDepth != newHookResults.size())
					Assert(!"Broken behavior: Someone called SetState too many times!");

				if (callOriginal)
					This()->GetOriginal()(args...);
			}
		};


	private:
		static IGroupHook* s_This;
	};

	template<class FuncEnumType, FuncEnumType hookID, class FunctionalType, class RetVal, class... Args>
	IGroupHook* BaseGroupHook<FuncEnumType, hookID, FunctionalType, RetVal, Args...>::s_This = nullptr;

	template<class FuncEnumType, FuncEnumType hookID, class FunctionalType, class RetVal, class... Args>
	thread_local std::stack<HookAction>* BaseGroupHook<FuncEnumType, hookID, FunctionalType, RetVal, Args...>::s_HookResults = nullptr;

	template<class FuncEnumType, FuncEnumType hookID, class FunctionalType, class RetVal, class... Args>
	inline void BaseGroupHook<FuncEnumType, hookID, FunctionalType, RetVal, Args...>::SetState(HookAction action)
	{
		Assert(s_HookResults);
		if (s_HookResults)
			s_HookResults->push(action);
	}

	template<class FuncEnumType, FuncEnumType hookID, class FunctionalType, class RetVal, class... Args>
	inline int BaseGroupHook<FuncEnumType, hookID, FunctionalType, RetVal, Args...>::AddHook(const Functional& newHook)
	{
		const auto newIndex = ++s_LastHook;

		// Make sure we're initialized
		InitHook();

		// Now add the new hook
		{
			std::unique_lock<std::shared_mutex> lock(m_HooksTableMutex);
			m_HooksTable.insert(std::make_pair(newIndex, newHook));
		}

		return newIndex;
	}

	template<class FuncEnumType, FuncEnumType hookID, class FunctionalType, class RetVal, class... Args>
	inline bool BaseGroupHook<FuncEnumType, hookID, FunctionalType, RetVal, Args...>::RemoveHook(int hookID, const char* funcName)
	{
		std::unique_lock<std::shared_mutex> lock(m_HooksTableMutex);

		auto found = m_HooksTable.find(hookID);
		if (found == m_HooksTable.end())
		{
			PluginWarning("Function %s called %s with invalid hook ID %i!\n", funcName, __FUNCSIG__, hookID);
			return false;
		}

		m_HooksTable.erase(found);

		return true;
	}

	template<class FuncEnumType, FuncEnumType hookID, class FunctionalType, class RetVal, class... Args>
	inline BaseGroupHook<FuncEnumType, hookID, FunctionalType, RetVal, Args...>::BaseGroupHook()
	{
		Assert(!s_This);
		s_This = this;
	}
	template<class FuncEnumType, FuncEnumType hookID, class FunctionalType, class RetVal, class... Args>
	inline BaseGroupHook<FuncEnumType, hookID, FunctionalType, RetVal, Args...>::~BaseGroupHook()
	{
		Assert(!m_HooksTable.size());
		Assert(s_This == this);
		s_This = nullptr;
	}

	template<class FuncEnumType, FuncEnumType hookID, class FunctionalType, class RetVal, class... Args>
	template<class _RetVal>
	inline _RetVal BaseGroupHook<FuncEnumType, hookID, FunctionalType, RetVal, Args...>::HookFunctionsInvoker<_RetVal>::Invoke(Args... args)
	{
		// Run all the hooks
		std::shared_lock<std::shared_mutex> lock(This()->m_HooksTableMutex);

		std::stack<HookAction> newHookResults;
		auto hookResultsPusher = CreateVariablePusher(s_HookResults, &newHookResults);

		const auto outerStartDepth = newHookResults.size();
		int index = 0;
		int retValIndex = -1;
		RetVal retVal = RetVal();
		for (auto& currentHook : This()->m_HooksTable)
		{
			const auto startDepth = newHookResults.size();
			const auto& temp = std::move(currentHook.second(args...));

			if (startDepth == newHookResults.size())
				newHookResults.push(HookAction::IGNORE);

			switch (newHookResults.top())
			{
				case HookAction::IGNORE:	break;
				case HookAction::SUPERCEDE:
				{
					if (retValIndex != -1)
						AssertMsg(temp == retVal, "Someone else already used HookAction::SUPERCEDE this hook with a different return value!");

					retVal = temp;
					retValIndex = index;
					break;
				}
				default:	Assert(!"Invalid HookAction?");
			}

			newHookResults.pop();
			index++;
		}

		if (retValIndex >= 0)
			return retVal;

		if (outerStartDepth != newHookResults.size())
			Assert(!"Broken behavior: Someone called SetState too many times!");

		return This()->GetOriginal()(args...);
	}
}