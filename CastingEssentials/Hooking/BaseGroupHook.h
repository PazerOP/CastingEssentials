#pragma once
#include "IGroupHook.h"

#include <map>
#include <stack>

namespace Hooking
{
	template<class FuncEnumType, FuncEnumType hookID, class RetVal, class... Args>
	class BaseGroupHook : public IGroupHook
	{
		static_assert(std::is_enum<FuncEnumType>::value, "FuncEnumType must be an enum!");
		static constexpr FuncEnumType Dumb() { return hookID; }
	public:
		typedef FuncEnumType FuncEnumType;
		static constexpr FuncEnumType HOOK_ID = Dumb();
		typedef RetVal RetVal;

		typedef std::function<RetVal(Args...)> Functional;
		typedef BaseGroupHook<FuncEnumType, hookID, RetVal, Args...> SelfType;
		typedef SelfType BaseGroupHookType;

		virtual void SetState(HookAction action) override;

		virtual int AddHook(const Functional& newHook);
		virtual bool RemoveHook(int hookID, const char* funcName);
		virtual Functional GetOriginal() = 0;

	protected:
		BaseGroupHook();
		virtual ~BaseGroupHook();

		std::recursive_mutex m_HooksTableMutex;
		std::map<uint64, Functional> m_HooksTable;
		static thread_local std::stack<HookAction>* s_HookResults;

		template<class _RetVal> struct Stupid { static _RetVal InvokeHookFunctions(Args... args); };
		template<> struct Stupid<void>
		{
			static void InvokeHookFunctions(Args... args)
			{
				std::lock_guard<std::recursive_mutex> lock(This()->m_HooksTableMutex);

				std::stack<HookAction>* oldHookResults = s_HookResults;
				std::stack<HookAction> newHookResults;
				s_HookResults = &newHookResults;
				{
					const auto outerStartDepth = s_HookResults->size();
					bool callOriginal = true;
					auto& hooksTable = This()->m_HooksTable;
					for (auto hook : hooksTable)
					{
						const auto startDepth = s_HookResults->size();

						hook.second(args...);

						if (startDepth == s_HookResults->size())
							s_HookResults->push(HookAction::IGNORE);

						Assert(s_HookResults->size());
						switch (s_HookResults->top())
						{
							case HookAction::IGNORE:	break;
							case HookAction::SUPERCEDE:
							{
								callOriginal = false;
								break;
							}
							default:	Assert(!"Invalid HookAction?");
						}

						s_HookResults->pop();
					}

					if (outerStartDepth != s_HookResults->size())
						Assert(!"Broken behavior: Someone called SetState too many times!");

					if (callOriginal)
						This()->GetOriginal()(args...);
				}
				s_HookResults = oldHookResults;
			}
		};

		static SelfType* This() { return assert_cast<SelfType*>(BaseThis()); }
		static IGroupHook* BaseThis() { return s_This; }

	private:
		static IGroupHook* s_This;
	};

	template<class FuncEnumType, FuncEnumType hookID, class RetVal, class... Args>
	IGroupHook* BaseGroupHook<FuncEnumType, hookID, RetVal, Args...>::s_This = nullptr;

	template<class FuncEnumType, FuncEnumType hookID, class RetVal, class... Args>
	thread_local std::stack<HookAction>* BaseGroupHook<FuncEnumType, hookID, RetVal, Args...>::s_HookResults = nullptr;

	template<class FuncEnumType, FuncEnumType hookID, class RetVal, class... Args>
	inline void BaseGroupHook<FuncEnumType, hookID, RetVal, Args...>::SetState(HookAction action)
	{
		Assert(s_HookResults);
		if (s_HookResults)
			s_HookResults->push(action);
	}

	template<class FuncEnumType, FuncEnumType hookID, class RetVal, class... Args>
	inline int BaseGroupHook<FuncEnumType, hookID, RetVal, Args...>::AddHook(const Functional& newHook)
	{
		const auto newIndex = ++s_LastHook;

		// Make sure we're initialized
		InitHook();

		// Now add the new hook
		{
			std::lock_guard<std::recursive_mutex> lock(m_HooksTableMutex);
			m_HooksTable.insert(std::make_pair(newIndex, newHook));
		}

		return newIndex;
	}

	template<class FuncEnumType, FuncEnumType hookID, class RetVal, class... Args>
	inline bool BaseGroupHook<FuncEnumType, hookID, RetVal, Args...>::RemoveHook(int hookID, const char* funcName)
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

	template<class FuncEnumType, FuncEnumType hookID, class RetVal, class... Args>
	inline BaseGroupHook<FuncEnumType, hookID, RetVal, Args...>::BaseGroupHook()
	{
		Assert(!s_This);
		s_This = this;
	}
	template<class FuncEnumType, FuncEnumType hookID, class RetVal, class... Args>
	inline BaseGroupHook<FuncEnumType, hookID, RetVal, Args...>::~BaseGroupHook()
	{
		Assert(!m_HooksTable.size());
		Assert(s_This == this);
		s_This = nullptr;
	}

	template<class FuncEnumType, FuncEnumType hookID, class RetVal, class... Args>
	template<class _RetVal>
	inline _RetVal BaseGroupHook<FuncEnumType, hookID, RetVal, Args...>::Stupid<_RetVal>::InvokeHookFunctions(Args... args)
	{
		// Run all the hooks
		std::lock_guard<std::recursive_mutex> locker(This()->m_HooksTableMutex);

		std::stack<HookAction>* oldHookResults = s_HookResults;
		std::stack<HookAction> newHookResults;
		s_HookResults = &newHookResults;
		{
			const auto outerStartDepth = s_HookResults->size();
			int index = 0;
			int retValIndex = -1;
			_RetVal retVal = _RetVal();
			auto& hooksTable = This()->m_HooksTable;
			for (auto hook : hooksTable)
			{
				const auto startDepth = s_HookResults->size();
				const auto& temp = hook.second(args...);

				if (startDepth == s_HookResults->size())
					s_HookResults->push(HookAction::IGNORE);

				switch (s_HookResults->top())
				{
					case HookAction::IGNORE:	break;
					case HookAction::SUPERCEDE:
					{
						if (retValIndex != -1)
							Assert(!"Someone else already used HookAction::SUPERCEDE this hook!");

						retVal = temp;
						retValIndex = index;
						break;
					}
					default:	Assert(!"Invalid HookAction?");
				}

				s_HookResults->pop();
				index++;
			}

			if (retValIndex >= 0)
			{
				s_HookResults = oldHookResults;
				return retVal;
			}

			if (outerStartDepth != s_HookResults->size())
				Assert(!"Broken behavior: Someone called SetState too many times!");

			s_HookResults = oldHookResults;
			return This()->GetOriginal()(args...);
		}
	}
}