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
		static_assert(!std::is_reference<T>::value);
		static_assert(!std::is_const<T>::value);
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
} }