#include "ConsoleTools.h"

#include "Misc/CCvar.h"
#include "Misc/CmdAlias.h"
#include "Misc/SuggestionList.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"

#include <vprof.h>

#include <optional>
#include <regex>
#include <sstream>

#undef min
#undef max

MODULE_REGISTER(ConsoleTools);

class ConsoleTools::PauseFilter final
{
public:
	PauseFilter()
	{
		ConsoleTools* module = GetModule();
		if (!module)
			return;

		module->m_FilterPaused++;
	}
	~PauseFilter()
	{
		ConsoleTools* module = GetModule();
		if (!module)
			return;

		module->m_FilterPaused--;
		Assert(module->m_FilterPaused >= 0);
	}
};

ConsoleTools::ConsoleTools() :
	ce_consoletools_filter_enabled("ce_consoletools_filter_enabled", "0", FCVAR_NONE, "Enables or disables console filtering.",
		[](IConVar* var, const char*, float) { GetModule()->ToggleFilterEnabled(static_cast<ConVar*>(var)); }),
	ce_consoletools_filter_add("ce_consoletools_filter_add", [](const CCommand& cmd) { GetModule()->AddFilter(cmd); },
		"Adds a new console filter. Uses regular expressions."),
	ce_consoletools_filter_remove("ce_consoletools_filter_remove", [](const CCommand& cmd) { GetModule()->RemoveFilter(cmd); },
		"Removes an existing console filter."),
	ce_consoletools_filter_list("ce_consoletools_filter_list", [](const CCommand& cmd) { GetModule()->ListFilters(cmd); }, "Lists all console filters."),

	ce_consoletools_flags_add("ce_consoletools_flags_add", m_AddFlagsCallbacks, "Adds a flag to a cvar.", FCVAR_NONE, m_AddFlagsCallbacks),
	ce_consoletools_flags_remove("ce_consoletools_flags_remove", m_RemoveFlagsCallbacks, "Removes a flag from a cvar.", FCVAR_NONE, m_RemoveFlagsCallbacks),

	ce_consoletools_limits_set("ce_consoletools_limits_set", m_SetLimitsCallbacks, "Sets or removes limits for a ConVar.", FCVAR_NONE, m_SetLimitsCallbacks),

	ce_consoletools_alias_remove("ce_consoletools_alias_remove", m_RemoveAliasCallbacks,
		"Removes an existing alias created with the \"alias\" command.", FCVAR_NONE, m_RemoveAliasCallbacks),
	ce_consoletools_unhide_all_cvars("ce_consoletools_unhide_all_cvars", UnhideAllCvars,
		"Removes FCVAR_DEVELOPMENTONLY and FCVAR_HIDDEN from all cvars."),

	m_ConsoleColorPrintfHook(std::bind(&ConsoleTools::ConsoleColorPrintfHook, this, std::placeholders::_1, std::placeholders::_2)),
	m_ConsoleDPrintfHook(std::bind(&ConsoleTools::ConsoleDPrintfHook, this, std::placeholders::_1)),
	m_ConsolePrintfHook(std::bind(&ConsoleTools::ConsolePrintfHook, this, std::placeholders::_1)),

	m_SetLimitsCallbacks(&SetLimits, &SetLimitsAutocomplete),
	m_AddFlagsCallbacks(&AddFlags, &FlagModifyAutocomplete),
	m_RemoveFlagsCallbacks(&RemoveFlags, &FlagModifyAutocomplete),
	m_RemoveAliasCallbacks(&RemoveAlias, &RemoveAliasAutocomplete)
{
	m_FilterPaused = false;
}

bool ConsoleTools::CheckDependencies()
{
	bool ready = true;

	if (!g_pCVar)
	{
		PluginWarning("Required interface ICVar for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!GetHooks()->GetHook<HookFunc::ICvar_ConsoleColorPrintf>())
	{
		PluginWarning("Required hook ICvar::ConsoleColorPrintf for module %s not available!\n", GetModuleName());
		ready = false;
	}
	if (!GetHooks()->GetHook<HookFunc::ICvar_ConsoleDPrintf>())
	{
		PluginWarning("Required hook ICvar::ConsoleDPrintf for module %s not available!\n", GetModuleName());
		ready = false;
	}
	if (!GetHooks()->GetHook<HookFunc::ICvar_ConsolePrintf>())
	{
		PluginWarning("Required hook ICvar::ConsolePrintf for module %s not available!\n", GetModuleName());
		ready = false;
	}

	return ready;
}

void ConsoleTools::AddFilter(const CCommand &command)
{
	if (command.ArgC() >= 2)
	{
		const auto& insertionResult = m_Filters.emplace(std::make_pair(command[1],
			std::regex(command[1], std::regex_constants::ECMAScript | std::regex_constants::optimize)));

		if (!insertionResult.second)
		{
			PauseFilter pause;
			PluginWarning("Filter %s is already present.\n", command[1]);
		}
	}
	else
		PluginWarning("Usage: %s <filter>\n", command[0]);
}
void ConsoleTools::ToggleFilterEnabled(const ConVar *var)
{
	const bool enabled = var->GetBool();

	m_ConsolePrintfHook.SetEnabled(enabled);
	m_ConsoleDPrintfHook.SetEnabled(enabled);
	m_ConsoleColorPrintfHook.SetEnabled(enabled);
}

void ConsoleTools::UnhideAllCvars()
{
	static constexpr auto MASK = ~(FCVAR_DEVELOPMENTONLY | FCVAR_HIDDEN);

	for (auto var = g_pCVar->GetCommands(); var; var = var->GetNext())
		CCvar::SetFlags(var, CCvar::GetFlags(var) & MASK);
}

void ConsoleTools::SetLimits(const CCommand& command)
{
	if (command.ArgC() != 4)
		goto Usage;

	ConVar* var;
	if ((var = cvar->FindVar(command[1])) == nullptr)
	{
		Warning("Unable to find cvar named \"%s\"\n", command[1]);
		goto Usage;
	}

	float minMax[2];
	for (int i = 0; i < 2; i++)
	{
		if (command[2 + i][0] == 'x' && command[2 + i][1] == '\0')
			minMax[i] = NAN;
		else if (command[2 + i][0] == '?' && command[2 + i][1] == '\0')
		{
			float oldMinMaxVal;
			if ((i == 0 && var->GetMin(oldMinMaxVal)) || (i == 1 && var->GetMax(oldMinMaxVal)))
				minMax[i] = oldMinMaxVal;
		}
		else if (float parsed; TryParseFloat(command[2 + i], parsed))
			minMax[i] = parsed;
		else
			goto Usage;
	}

	CCvar::SetMin(var, minMax[0]);
	CCvar::SetMax(var, minMax[1]);
	return;

Usage:
	Warning("Usage: %s <cvar> <minimum/'x' for none/'?' for keep existing> <maximum/'x' for none/'?' for keep existing>\n", command[0]);
}

void ConsoleTools::SetLimitsAutocomplete(const CCommand& partial, CUtlVector<CUtlString>& outSuggestions)
{
	if (partial.ArgC() < 1 || partial.ArgC() > 2)
		return;

	const auto partialCvarLength = strlen(partial[1]);
	SuggestionList<> suggestions;

	for (const ConCommandBase* cmd = g_pCVar->GetCommands(); cmd; cmd = cmd->GetNext())
	{
		if (cmd->IsCommand())
			continue;  // Only care about actual ConVars

		const char* const cmdName = cmd->GetName();
		if (strnicmp(cmdName, partial[1], partialCvarLength))
			continue;

		suggestions.insert(cmdName);
	}

	// Copy suggestions into output array
	suggestions.EnsureSorted();
	for (const auto& suggestion : suggestions)
	{
		char buf[512];
		const auto length = sprintf_s(buf, "%s %s", partial[0], suggestion);
		outSuggestions.AddToTail(CUtlString(buf, length));
	}
}

void ConsoleTools::FlagModifyAutocomplete(const CCommand& partial, CUtlVector<CUtlString>& outSuggestions)
{
	if (partial.ArgC() < 1 || partial.ArgC() > 2)
		return;

	const auto arg1Length = partial.ArgC() > 1 ? strlen(partial[1]) : 0;

	SuggestionList<> suggestions;
	for (const ConCommandBase* cmd = g_pCVar->GetCommands(); cmd; cmd = cmd->GetNext())
	{
		const char* const cmdName = cmd->GetName();
		if (!strnicmp(cmdName, partial[1], arg1Length))
			suggestions.insert(cmdName);
	}

	// Copy suggestions into output array
	suggestions.EnsureSorted();
	for (const auto& suggestion : suggestions)
	{
		char buf[512];
		const auto length = sprintf_s(buf, "%s %s", partial[0], suggestion);
		outSuggestions.AddToTail(CUtlString(buf, length));
	}
}

void ConsoleTools::ConsoleColorPrintfHook(const Color &clr, const char *message)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (!m_FilterPaused && CheckFilters(message))
		GetHooks()->GetHook<HookFunc::ICvar_ConsoleColorPrintf>()->SetState(Hooking::HookAction::SUPERCEDE);
}

void ConsoleTools::ConsoleDPrintfHook(const char *message)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (!m_FilterPaused && CheckFilters(message))
		GetHooks()->GetHook<HookFunc::ICvar_ConsoleDPrintf>()->SetState(Hooking::HookAction::SUPERCEDE);
}

void ConsoleTools::ConsolePrintfHook(const char *message)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (!m_FilterPaused && CheckFilters(message))
		GetHooks()->GetHook<HookFunc::ICvar_ConsolePrintf>()->SetState(Hooking::HookAction::SUPERCEDE);
}

bool ConsoleTools::CheckFilters(const char* message) const
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	for (const auto& filter : m_Filters)
	{
		if (std::regex_search(message, filter.second))
			return true;
	}

	return false;
}

void ConsoleTools::RemoveFilter(const CCommand &command)
{
	if (command.ArgC() == 2)
	{
		if (!m_Filters.erase(command[1]))
			PluginWarning("Filter %s is not already present.\n", command.Arg(1));
	}
	else
		PluginWarning("Usage: %s <filter>\n", command[0]);
}

int ConsoleTools::ParseFlags(const CCommand& command)
{
	int retVal = 0;

	for (int i = 2; i < command.ArgC(); i++)
	{
		const char* flag = command.Arg(i);

		if (!stricmp(flag, "game") || !stricmp(flag, "gamedll") || !stricmp(flag, "FCVAR_GAMEDLL"))
			retVal |= FCVAR_GAMEDLL;
		else if (!stricmp(flag, "client") || !stricmp(flag, "clientdll") || !stricmp(flag, "FCVAR_CLIENT"))
			retVal |= FCVAR_CLIENTDLL;
		else if (!stricmp(flag, "archive") || !stricmp(flag, "FCVAR_ARCHIVE"))
			retVal |= FCVAR_ARCHIVE;
		else if (!stricmp(flag, "notify") || !stricmp(flag, "FCVAR_NOTIFY"))
			retVal |= FCVAR_NOTIFY;
		else if (!stricmp(flag, "singleplayer") || !stricmp(flag, "FCVAR_SPONLY"))
			retVal |= FCVAR_SPONLY;
		else if (!stricmp(flag, "notconnected") || !stricmp(flag, "FCVAR_NOT_CONNECTED"))
			retVal |= FCVAR_NOT_CONNECTED;
		else if (!stricmp(flag, "cheat") || !stricmp(flag, "FCVAR_CHEAT"))
			retVal |= FCVAR_CHEAT;
		else if (!stricmp(flag, "replicated") || !stricmp(flag, "FCVAR_REPLICATED"))
			retVal |= FCVAR_REPLICATED;
		else if (!stricmp(flag, "server_can_execute") || !stricmp(flag, "FCVAR_SERVER_CAN_EXECUTE"))
			retVal |= FCVAR_SERVER_CAN_EXECUTE;
		else if (!stricmp(flag, "clientcmd_can_execute") || !stricmp(flag, "FCVAR_CLIENTCMD_CAN_EXECUTE"))
			retVal |= FCVAR_CLIENTCMD_CAN_EXECUTE;
		else if (!stricmp(flag, "devonly") || !stricmp(flag, "developmentonly") || !stricmp(flag, "FCVAR_DEVELOPMENTONLY"))
			retVal |= FCVAR_DEVELOPMENTONLY;
		else if (!stricmp(flag, "hidden") || !stricmp(flag, "FCVAR_HIDDEN"))
			retVal |= FCVAR_HIDDEN;
		// Pazer: seems dangerous...
		//else if (IsInteger(flag))
		//{
		//	flags |= ~(1 << atoi(flag.c_str()));
		//}
		else
			PluginWarning("Unrecognized flag %s!\n", command.Arg(i));
	}

	return retVal;
}

void ConsoleTools::RemoveFlags(const CCommand &command)
{
	if (command.ArgC() >= 3)
	{
		const char *name = command.Arg(1);

		ConCommandBase *base = g_pCVar->FindCommandBase(name);

		if (base)
		{
			int flags = CCvar::GetFlags(base);

			int toRemove = ParseFlags(command);

			CCvar::SetFlags(base, flags & (~toRemove));
		}
		else
			PluginWarning("%s is not a valid command or variable!\n", command.Arg(1));
	}
	else
		PluginWarning("Usage: %s <name> <flag1> [flag2 ...]\n", command.Arg(0));
}

void ConsoleTools::RemoveAlias(const CCommand& command)
{
	if (command.ArgC() != 2)
	{
		Warning("Usage: %s <alias>", command[0]);
		return;
	}

	auto aliases = Interfaces::GetCmdAliases();
	if (!*aliases)
	{
		Warning("%s: No current registered aliases.\n", command[0]);
		return;
	}

	cmdalias_t** previous = aliases;
	for (auto alias = *aliases; alias; previous = &alias->next, alias = alias->next)
	{
		if (stricmp(alias->name, command[1]))
			continue;

		*previous = alias->next;
		delete alias;

		Msg("%s: Removed alias \"%s\".\n", command[0], command[1]);
		return;
	}

	Warning("%s: Unable to find alias named \"%s\"\n", command[0], command[1]);
}

void ConsoleTools::RemoveAliasAutocomplete(const CCommand& partial, CUtlVector<CUtlString>& outSuggestions)
{
	if (partial.ArgC() < 1 || partial.ArgC() > 2)
		return;

	// Number of characters after command name + 1 whitespace
	const auto arg1Length = strlen(partial[1]);

	SuggestionList<> suggestions;
	for (auto alias = *Interfaces::GetCmdAliases(); alias; alias = alias->next)
	{
		if (strnicmp(alias->name, partial[1], arg1Length))
			continue;

		suggestions.insert(alias->name);
	}

	suggestions.EnsureSorted();
	for (const auto& suggestion : suggestions)
	{
		char buf[512];
		const auto length = sprintf_s(buf, "%s %s", partial[0], suggestion);
		outSuggestions.AddToTail(CUtlString(buf, length));
	}
}

void ConsoleTools::AddFlags(const CCommand &command)
{
	if (command.ArgC() >= 3)
	{
		const char *name = command.Arg(1);

		ConCommandBase *base = g_pCVar->FindCommandBase(name);

		if (base)
		{
			int flags = CCvar::GetFlags(base);
			int toAdd = ParseFlags(command);
			CCvar::SetFlags(base, flags | toAdd);
		}
		else
			PluginWarning("%s is not a valid command or variable!\n", command.Arg(1));
	}
	else
		PluginWarning("Usage: %s <name> <flag1> [flag2 ...]\n", command.Arg(0));
}

void ConsoleTools::ListFilters(const CCommand& command)
{
	if (m_Filters.size() < 1)
	{
		PluginMsg("No active filters.\n");
		return;
	}

	std::stringstream ss;
	ss << m_Filters.size() << " console filters:\n";
	for (auto filter : m_Filters)
		ss << "     " << filter.first << '\n';

	{
		PauseFilter pause;
		PluginMsg(ss.str().c_str());
	}
}