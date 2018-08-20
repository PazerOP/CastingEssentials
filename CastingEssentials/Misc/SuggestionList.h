#pragma once

#include <convar.h>

#include <algorithm>
#include <array>
#include <string>

template<typename T = const char*, size_t MaxCount = COMMAND_COMPLETION_MAXITEMS>
class SuggestionList
{
public:
	auto begin() { return m_Array.begin(); }
	auto begin() const { return m_Array.begin(); }
	auto end() { return m_Array.begin() + m_Count; }
	auto end() const { return m_Array.begin() + m_Count; }

	auto size() const { return m_Count; }

	void insert(const T& val) { emplace(std::move(T(val))); }
	void emplace(T&& val)
	{
		if (m_Count < MaxCount)
		{
			// Just add to the end, we'll sort later
			m_Array[m_Count++] = std::move(val);
		}
		else
		{
			// Make sure we are initially sorted
			EnsureSorted();

			// Insertion sort
			const auto& upperBound = std::upper_bound(m_Array.begin(), m_Array.end(), val, Sorter{});
			if (upperBound != m_Array.end())
			{
				std::move(upperBound, m_Array.end() - 1, upperBound + 1);
				*upperBound = std::move(val);
			}
		}
	}

	void EnsureSorted()
	{
		// If we're not sorted, sort
		if (!m_Sorted)
		{
			std::sort(begin(), end(), Sorter{});
			m_Sorted = true;
		}
	}

	void clear()
	{
		m_Sorted = false;
		m_Count = 0;
	}

private:
	struct CaseInsensitiveTraits : public std::char_traits<char>
	{
		static constexpr bool eq(char a, char b) { return toupper(a) == toupper(b); }
		static constexpr bool lt(char a, char b) { return toupper(a) < toupper(b); }
	};

	struct Sorter
	{
		bool operator()(const std::basic_string_view<char, CaseInsensitiveTraits>& a,
			const std::basic_string_view<char, CaseInsensitiveTraits>& b) const
		{
			return a < b;
		}
	};

	bool m_Sorted = false;
	size_t m_Count = 0;
	std::array<T, MaxCount> m_Array;
};