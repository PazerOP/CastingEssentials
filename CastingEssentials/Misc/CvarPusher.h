#pragma once

#include <convar.h>

class CvarPusher
{
public:
	CvarPusher(const char* name, const char* newValue) : m_Var(name) { Push(newValue); }
	CvarPusher(const char* name, bool newValue) : m_Var(name) { Push(newValue ? "1" : "0"); }
	CvarPusher(const char* name, float newValue) : m_Var(name)
	{
		char buf[32];
		sprintf_s(buf, "%f", newValue);
		Push(buf);
	}
	CvarPusher(const char* name, int newValue) : m_Var(name)
	{
		char buf[32];
		sprintf_s(buf, "%i", newValue);
		Push(buf);
	}
	~CvarPusher()
	{
		if (m_OldString) // If we don't have this, we are already clear
			m_Var.SetValue(m_OldString.get());
	}

	CvarPusher(const CvarPusher&) = delete;
	CvarPusher& operator=(const CvarPusher&) = delete;

	void Clear() { m_OldString.reset(); }

	const char* GetName() const { Assert(m_OldString); return m_Var.GetName(); }

	const char* GetOldString() const { Assert(m_OldString); return m_OldString.get(); }
	float GetOldFloat() const { Assert(m_OldString);  return atof(m_OldString.get()); }
	int GetOldInt() const { Assert(m_OldString); return atoi(m_OldString.get()); }
	bool GetOldBool() const { Assert(m_OldString); return !(m_OldString[0] == '0' && m_OldString[1] == '\0'); }

private:
	void Push(const char* newValue)
	{
		auto oldLen = strlen(m_Var.GetString());
		m_OldString = std::make_unique<char[]>(oldLen);
		memcpy(m_OldString.get(), m_Var.GetString(), oldLen);
		m_OldString[oldLen] = '\0';
	}

	ConVarRef m_Var;
	std::unique_ptr<char[]> m_OldString;
};
