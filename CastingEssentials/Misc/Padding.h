#pragma once

#include <cstddef>

template<size_t i> struct _PADDING_HELPER : _PADDING_HELPER<i - 1>
{
	static_assert(i != 0, "0 size struct not supported in C++");
	std::byte : 8;
};
template<> struct _PADDING_HELPER<1>
{
	std::byte : 8;
};
template<> struct _PADDING_HELPER<size_t(-1)>
{
	// To catch infinite recursion
};

#define PADDING(size) _PADDING_HELPER<size> EXPAND_CONCAT(CE_PADDING, __COUNTER__)