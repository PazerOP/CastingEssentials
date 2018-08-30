#pragma once
#include "PluginBase/Common.h"

#include <convar.h>

#include <functional>

class CommandCallbacks : ICommandCallback, ICommandCompletionCallback
{
public:
	CommandCallbacks(std::function<void(const CCommand& cmd)>&& dispatch) :
		m_Dispatch(std::move(dispatch))
	{
	}
	CommandCallbacks(std::function<void(const CCommand& cmd)>&& dispatch,
		std::function<void(const CCommand& partial, CUtlVector<CUtlString>& suggestions)>&& autocomplete) :
		m_Dispatch(std::move(dispatch)), m_Autocomplete(std::move(autocomplete))
	{
	}

	operator ICommandCallback*() { return this; }
	operator ICommandCompletionCallback*() { return m_Autocomplete ? this : nullptr; }

private:
	void CommandCallback(const CCommand& cmd) override final
	{
		m_Dispatch(cmd);
	}
	int CommandCompletionCallback(const char* partial, CUtlVector<CUtlString>& suggestions) override final
	{
		CCommand cmd;
		cmd.Tokenize(partial);
		m_Autocomplete(cmd, suggestions);
		Assert(suggestions.Count() <= COMMAND_COMPLETION_MAXITEMS);
		return suggestions.Count();
	}

	std::function<void(const CCommand& partial)> m_Dispatch;
	std::function<void(const CCommand& partial, CUtlVector<CUtlString>& suggestions)> m_Autocomplete;
};