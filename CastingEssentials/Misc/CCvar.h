#pragma once

#include <convar.h>

#include <variant>

class CCvar
{
public:
	static int GetFlags(const ConCommandBase* base) { return base->m_nFlags; }
	static void SetFlags(ConCommandBase* base, int flags) { base->m_nFlags = flags; }

	static void SetMin(ConVar* var, float min)
	{
		if ((var->m_bHasMin = !std::isnan(min)) == true)
			var->m_fMinVal = min;
	}
	static void SetMax(ConVar* var, float max)
	{
		if ((var->m_bHasMax = !std::isnan(max)) == true)
			var->m_fMaxVal = max;
	}

	static std::variant<FnCommandCallbackVoid_t*, FnCommandCallback_t*, ICommandCallback**> GetDispatchCallback(ConCommand* cmd)
	{
		if (cmd->m_bUsingNewCommandCallback)
			return &cmd->m_fnCommandCallback;
		else if (cmd->m_bUsingCommandCallbackInterface)
			return &cmd->m_pCommandCallback;
		else
			return &cmd->m_fnCommandCallbackV1;
	}
	static std::variant<FnCommandCompletionCallback*, ICommandCompletionCallback**> GetCompletionCallback(ConCommand* cmd)
	{
		if (cmd->m_bUsingCommandCallbackInterface)
			return &cmd->m_pCommandCompletionCallback;
		else
			return &cmd->m_fnCompletionCallback;
	}

	static FnChangeCallback_t* GetChangeCallback(ConVar* var)
	{
		return &var->m_fnChangeCallback;
	}
	static FnChangeCallback_t* GetChangeCallbackClient(ConVar* var)
	{
		return &var->m_fnChangeCallbackClient;
	}
};
