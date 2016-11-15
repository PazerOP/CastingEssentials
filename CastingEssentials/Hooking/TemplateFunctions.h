#pragma once
#include "TemplateTypedefs.h"

#include <mutex>
#include <stack>
#include <type_traits>

namespace Hooking{ namespace Internal
{
	template<class OutType, class InType> static OutType evil_cast(InType input)
	{
		static_assert(std::is_pointer<InType>::value, "InType must be a pointer type!");
		static_assert(std::is_pointer<OutType>::value, "OutType must be a pointer type!");
		static_assert(sizeof(InType) == sizeof(OutType), "InType and OutType are not the same size!");

		// >:(
		union
		{
			InType goodIn;
			OutType evilOut;
		};

		goodIn = input;
		return evilOut;
	}

	template<typename T> struct ArgType
	{
		static_assert(!std::is_reference<T>::value, "error");
		static_assert(!std::is_const<T>::value, "error");
		typedef T type;
	};
	template<typename T> struct ArgType<T&>
	{
		typedef typename std::remove_reference<T>::type* type;
	};

	template<class First, class Second, class... Others>
	struct Skip2Types
	{
		typedef First First;
		typedef Second Second;
		typedef std::tuple<Others...> Others;
	};

	template<size_t _First, size_t _Second, size_t... _Others>
	struct Skip2Indices
	{
	private:
		static constexpr size_t Dumb1() { return _First; }
		static constexpr size_t Dumb2() { return _Second; }
	public:

		static constexpr size_t First = Dumb1();
		static constexpr size_t Second = Dumb2();
		typedef std::index_sequence<_Others...> Others;
	};

	template<class GroupHookType, class RetVal, class... Args>
	struct HookFunctionsInvoker final
	{
		static RetVal Invoke(GroupHookType* hook, Args... args)
		{
			Assert(hook);

			// Run all the hooks
			std::lock_guard<std::recursive_mutex> locker(hook->m_HooksTableMutex);

			std::stack<HookAction>* const oldHookResults = GroupHookType::s_HookResults;
			std::stack<HookAction> newHookResults;
			GroupHookType::s_HookResults = &newHookResults;
			{
				const auto outerStartDepth = newHookResults.size();
				int index = 0;
				int retValIndex = -1;
				RetVal retVal = RetVal();
				auto& hooksTable = hook->m_HooksTable;
				for (auto currentHook : hooksTable)
				{
					const auto startDepth = newHookResults.size();
					const auto& temp = currentHook.second(args...);

					if (startDepth == newHookResults.size())
						newHookResults.push(HookAction::IGNORE);

					switch (newHookResults.top())
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

					newHookResults.pop();
					index++;
				}

				if (retValIndex >= 0)
				{
					GroupHookType::s_HookResults = oldHookResults;
					return retVal;
				}

				if (outerStartDepth != newHookResults.size())
					Assert(!"Broken behavior: Someone called SetState too many times!");

				GroupHookType::s_HookResults = oldHookResults;
				return hook->GetOriginal()(args...);
			}
		}
	};

	template<class GroupHookType, class... Args>
	struct HookFunctionsInvoker<GroupHookType, void, Args...>
	{
		static void Invoke(GroupHookType* hook, Args... args)
		{
			std::lock_guard<std::recursive_mutex> lock(hook->m_HooksTableMutex);

			std::stack<HookAction>* const oldHookResults = GroupHookType::s_HookResults;
			std::stack<HookAction> newHookResults;
			GroupHookType::s_HookResults = &newHookResults;
			{
				const auto outerStartDepth = newHookResults.size();
				bool callOriginal = true;
				auto& hooksTable = hook->m_HooksTable;
				for (auto currentHook : hooksTable)
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
					hook->GetOriginal()(args...);
			}
			GroupHookType::s_HookResults = oldHookResults;
		}
	};

	template<class GroupHookType, class Type, class RetVal, class... Args>
	LocalDetourFnPtr<Type, RetVal, Args...> LocalDetourFn(GroupHookType* hook)
	{
		static GroupHookType* s_Hook;
		s_Hook = hook;
		Assert(s_Hook);

		LocalDetourFnPtr<Type, RetVal, Args...> fn = [](Type* pThis, void*, Args... args)
		{
			return HookFunctionsInvoker<GroupHookType::BaseGroupHookType, RetVal, Args...>::Invoke(s_Hook, args...);
		};
		return fn;
	}

	template<class GroupHookType, class Type, class RetVal, class... Args>
	LocalVaArgsFnPtr<Type, RetVal, Args...> LocalVaArgsDetourFn(GroupHookType* hook)
	{
		static GroupHookType* s_Hook;
		s_Hook = hook;
		Assert(s_Hook);

		LocalVaArgsFnPtr<Type, RetVal, Args...> fn = [](Type* pThis, Args... args, ...)
		{
			Assert(s_Hook);

			constexpr std::size_t fmtParameter = sizeof...(Args)-1;
			using FmtType = typename std::tuple_element<fmtParameter, std::tuple<Args...>>::type;
			static_assert(std::is_same<FmtType, const char*>::value || std::is_same<FmtType, char*>::value, "Invalid format string type!");

			std::tuple<Args...> blah(args...);

			// Fuck you, type system
			const char** fmt = evil_cast<const char**>(&std::get<fmtParameter>(blah));

			va_list vaArgList;
			va_start(vaArgList, pThis);

			char** parameters[] = { (char**)(&(va_arg(vaArgList, ArgType<Args>::type)))... };

			// 8192 is the length used internally by CCvar
			char buffer[8192];
			vsnprintf_s(buffer, _TRUNCATE, *fmt, vaArgList);
			va_end(vaArgList);

			// Can't have variable arguments in std::function, overwrite the "format" parameter with
			// the fully parsed buffer
			*parameters[fmtParameter] = &buffer[0];

			// Now run all the hooks
			return HookFunctionsInvoker<GroupHookType::BaseGroupHookType, RetVal, Args...>::Invoke(s_Hook, args...);
		};
		return fn;
	}
} }