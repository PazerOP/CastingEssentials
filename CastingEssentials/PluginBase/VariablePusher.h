#pragma once

template<class T>
class VariablePusher final
{
public:
	constexpr VariablePusher() : m_Variable(nullptr) {}
	VariablePusher(const VariablePusher<T>& other) = delete;
	VariablePusher(VariablePusher<T>&& other)
	{
		m_Variable = std::move(other.m_Variable);
		other.m_Variable = nullptr;
		m_OldValue = std::move(other.m_OldValue);
	}
	VariablePusher& operator=(const VariablePusher<T>& other) = delete;
	VariablePusher& operator=(VariablePusher<T>&& other)
	{
		Clear();
		m_Variable = std::move(other.m_Variable);
		other.m_Variable = nullptr;
		m_OldValue = std::move(other.m_OldValue);
	}
	VariablePusher(T& variable, const T& newValue) : m_Variable(&variable)
	{
		m_OldValue = std::move(*m_Variable);
		*m_Variable = newValue;
	}
	VariablePusher(T& variable, T&& newValue) : m_Variable(&variable)
	{
		m_OldValue = std::move(*m_Variable);
		*m_Variable = std::move(newValue);
	}
	~VariablePusher()
	{
		if (m_Variable)
			*m_Variable = std::move(m_OldValue);
	}

	__forceinline constexpr T& GetOldValue() { return m_OldValue; }
	__forceinline constexpr const T& GetOldValue() const { return m_OldValue; }

	__forceinline constexpr bool IsEmpty() const { return !m_Variable; }
	constexpr void Clear()
	{
		if (m_Variable)
		{
			*m_Variable = std::move(m_OldValue);
			m_Variable = nullptr;
		}
	}

private:
	T* const m_Variable;
	T m_OldValue;
};

template<class T> inline VariablePusher<T> CreateVariablePusher(T& variable, T&& newValue)
{
	return VariablePusher<T>(variable, newValue);
}
template<class T> inline VariablePusher<T> CreateVariablePusher(T& variable, const T& newValue)
{
	return VariablePusher<T>(variable, newValue);
}