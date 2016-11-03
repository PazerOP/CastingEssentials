#pragma once
namespace Hooking{ namespace Internal
{
	template<class T>
	class VariablePusher final
	{
	public:
		VariablePusher(T** variable, T* value)
		{
			m_Variable = variable;
			m_OldValue = *m_Variable;
			*m_Variable = value;
		}
		~VariablePusher()
		{
			*m_Variable = m_OldValue;
		}

	private:
		T** m_Variable;
		T* m_OldValue;
	};
} }