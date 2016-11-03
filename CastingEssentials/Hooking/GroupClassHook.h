#pragma once
#include "GroupGlobalHook.h"

namespace Hooking
{
	template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class OriginalFnType, class DetourFnType, class Type, class RetVal, class... Args>
	class BaseGroupClassHook : public BaseGroupGlobalHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, RetVal, Args...>
	{
	public:
		typedef BaseGroupClassHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, Type, RetVal, Args...> BaseGroupClassHookType;
		typedef BaseGroupClassHookType SelfType;
		typedef BaseGroupGlobalHookType BaseType;

		BaseGroupClassHook(Type* instance, OriginalFnType fn, DetourFnType detour = nullptr) : BaseType(fn, detour)
		{
			m_Instance = instance;
			Assert(m_Instance);
		}

		virtual Functional GetOriginal() override { return GetOriginalImpl(std::index_sequence_for<Args...>{}); }
		virtual HookType GetType() const override { return HookType::Class; }

	protected:
		Type* m_Instance;

		BaseGroupClassHook() = delete;
		BaseGroupClassHook(const SelfType& other) = delete;

		static SelfType* This() { return assert_cast<SelfType*>(BaseThis()); }

		BaseGroupClassHook(ConstructorParam1* instance, ConstructorParam2* detour) : BaseType((ConstructorParam1*)detour)
		{
			m_Instance = (Type*)instance;
			Assert(m_Instance);
		}

		void SetDefaultPatchFn()
		{
			auto patchFn = [](OriginalFnType func, Type* instance, Args... args)
			{
				Assert(1);
				return func(instance, args...);
			};

			m_PatchFunction = std::bind(patchFn, std::placeholders::_1, m_Instance, std::placeholders::_2);
		}

		static DetourFnType StandardDetourFn()
		{
			DetourFnType detourFn = [](Type* pThis, void* edx, Args... args)
			{
				//Assert(pThis == This()->m_Instance);
				//dummy = nullptr;
				//return This()->GetOriginal()(args...);
				return Stupid<RetVal>::InvokeHookFunctions(args...);
			};
			return detourFn;
		}

		template<std::size_t... Is> Functional GetOriginalImpl(std::index_sequence<Is...>)
		{
			Assert(GetType() == HookType::Class || GetType() == HookType::Virtual);

			// Make sure we're initialized so we don't have any nasty race conditions
			InitHook();

			if (m_BaseHook)
			{
				OriginalFnType originalFnPtr = reinterpret_cast<OriginalFnType>(m_BaseHook->GetOriginalFunction());
				auto patch = [](OriginalFnType oFn, Args... args) { return oFn(This()->m_Instance, args...); };
				return std::bind(patch, originalFnPtr, (std::_Ph<(int)(Is + 1)>{})...);
			}
			else
			{
				AssertMsg(0, "Should never get here... hook should be initialized so we don't have a potential race condition!");
				return nullptr;
			}
		}

		static DetourFnType VaArgsDetourFn()
		{
			DetourFnType detourFn = [](Type* pThis, Args... args, ...)
			{
				constexpr std::size_t fmtParameter = sizeof...(Args)-1;
				using FmtType = typename std::tuple_element<fmtParameter, std::tuple<Args...>>::type;
				static_assert(!vaArgs || std::is_same<FmtType, const char*>::value || std::is_same<FmtType, char*>::value, "Invalid format string type!");

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
				return Stupid<RetVal>::InvokeHookFunctions(args...);
			};
			return detourFn;
		}
	};

	template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args> class GroupClassHook;

	// Non variable-arguments version
	template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
	class GroupClassHook<FuncEnumType, hookID, false, Type, RetVal, Args...> :
		public BaseGroupClassHook<FuncEnumType, hookID, false, Internal::LocalFnPtr<Type, RetVal, Args...>, Internal::LocalDetourFnPtr<Type, RetVal, Args...>, Type, RetVal, Args...>
	{
	public:
		typedef BaseGroupClassHookType BaseType;

		GroupClassHook(Type* instance, OriginalFnType function, DetourFnType detour = nullptr) : BaseType(instance, function, detour) { }

	private:
		DetourFnType DefaultDetourFn() override { return StandardDetourFn(); }
	};

	// Variable arguments version
	template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
	class GroupClassHook<FuncEnumType, hookID, true, Type, RetVal, Args...> :
		public BaseGroupClassHook<FuncEnumType, hookID, true, Internal::LocalVaArgsFnPtr<Type, RetVal, Args...>, Internal::LocalVaArgsFnPtr<Type, RetVal, Args...>, Type, RetVal, Args...>
	{
	public:
		typedef BaseGroupClassHookType BaseType;

	private:
		DetourFnType DefaultDetourFn() override { return VaArgsDetourFn(); }
	};
}