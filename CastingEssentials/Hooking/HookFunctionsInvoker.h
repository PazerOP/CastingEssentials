#pragma once
#include "IGroupHook.h"

#include <mutex>
#include <stack>

namespace Hooking{ namespace Internal
{
	template<class HookType, class Type, class RetVal, class... Args>
	struct HookFunctionsInvoker
	{
		template<class _RetVal> struct Stupid
		{
			static inline bool InvokeHookFunctions(std::stack<HookAction>& results, std::map<uint64, typename HookType::Functional>& hooksTable,
				_RetVal* retVal, Type* instance, Args... args)
			{
				// Run all the hooks
				int retValIndex = -1;

				int index = 0;
				for (auto hook : hooksTable)
				{
					const auto startDepth = results.size();
					const auto temp = std::move(hook.second(args...));

					if (startDepth == results.size())
						results.push(HookAction::IGNORE);

					switch (results.top())
					{
						case HookAction::IGNORE:	break;
						case HookAction::SUPERCEDE:
						{
							if (retValIndex != -1)
								Assert(!"Someone else already used HookAction::SUPERCEDE this hook!");

							*retVal = temp;
							retValIndex = index;
							break;
						}
						default:	Assert(!"Invalid HookAction?");
					}

					results.pop();
					index++;
				}

				if (results.size())
					Assert(!"Broken behavior: Someone called SetState too many times!");

				if (retValIndex >= 0)
					return false;

				return true;
			}
		};

		template<> struct Stupid<void>
		{
			static inline bool InvokeHookFunctions(Args... args)
			{
				bool callOriginal = true;
				std::stack<HookAction>* oldStack = s_ResultStack;
				std::stack<HookAction> currentStack;
				s_ResultStack = &currentStack;
				{
					for (auto hook : This()->m_HooksTable)
					{
						const auto startDepth = currentStack.size();

						hook.second(args...);

						if (startDepth == currentStack.size())
							currentStack.push(HookAction::IGNORE);

						switch (currentStack.top())
						{
							case HookAction::IGNORE:	break;
							case HookAction::SUPERCEDE:
							{
								callOriginal = false;
								break;
							}
							default:	Assert(!"Invalid HookAction?");
						}

						currentStack.pop();
					}

					if (currentStack.size())
						Assert(!"Broken behavior: Someone called SetState too many times!");
				}
				s_ResultStack = oldStack;

				if (callOriginal)
					This()->GetOriginal()(args...);
			}
		};
	};
} }