#include "ConsoleTools.h"

#include "Misc/CmdAlias.h"
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

	ce_consoletools_flags_add("ce_consoletools_flags_add", [](const CCommand& cmd) { GetModule()->AddFlags(cmd); },
		"Adds a flag to a cvar.", FCVAR_NONE, FlagModifyAutocomplete),
	ce_consoletools_flags_remove("ce_consoletools_flags_remove", [](const CCommand& cmd) { GetModule()->RemoveFlags(cmd); },
		"Removes a flag from a cvar.", FCVAR_NONE, FlagModifyAutocomplete),

	ce_consoletools_limits_set("ce_consoletools_limits_set", SetLimits, "Sets or removes limits for a ConVar.", FCVAR_NONE, SetLimitsAutocomplete),

	ce_consoletools_alias_remove("ce_consoletools_alias_remove", RemoveAlias,
		"Removes an existing alias created with the \"alias\" command.", FCVAR_NONE, RemoveAliasAutocomplete),

	m_ConsoleColorPrintfHook(std::bind(&ConsoleTools::ConsoleColorPrintfHook, this, std::placeholders::_1, std::placeholders::_2)),
	m_ConsoleDPrintfHook(std::bind(&ConsoleTools::ConsoleDPrintfHook, this, std::placeholders::_1)),
	m_ConsolePrintfHook(std::bind(&ConsoleTools::ConsolePrintfHook, this, std::placeholders::_1))
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

// this is a total hijack of a friend class declared but never defined in the public SDK
class CCvar
{
public:
	static int GetFlags(ConCommandBase *base) { return base->m_nFlags; }
	static void SetFlags(ConCommandBase *base, int flags) { base->m_nFlags = flags; }
	static void SetMin(ConVar* var, const std::optional<float>& min)
	{
		if (!min.has_value())
			var->m_bHasMin = false;
		else
		{
			var->m_bHasMin = true;
			var->m_fMinVal = min.value();
		}
	}
	static void SetMax(ConVar* var, const std::optional<float>& max)
	{
		if (!max.has_value())
			var->m_bHasMax = false;
		else
		{
			var->m_bHasMax = true;
			var->m_fMaxVal = max.value();
		}
	}
};

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

	std::optional<float> minMax[2];
	for (int i = 0; i < 2; i++)
	{
		if (command[2 + i][0] == 'x' && command[2 + i][1] == '\0')
			continue;
		else if (command[2 + i][0] == '?' && command[2 + i][1] == '\0')
		{
			float val;
			if ((i == 0 && var->GetMin(val)) || (i == 1 && var->GetMax(val)))
				minMax[i] = val;
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

int ConsoleTools::SetLimitsAutocomplete(const char* partial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH])
{
	CCommand partialParsed;
	partialParsed.Tokenize(partial);

	if (partialParsed.ArgC() < 1 || partialParsed.ArgC() > 2)
		return 0;

	const auto cmdNameLength = strlen(partialParsed[0]);
	const auto partialCvarLength = partialParsed.ArgC() > 1 ? strlen(partialParsed[1]) : 0;

	bool suggestionsSorted = false;
	static const auto suggestionsSorter = [](const char* a, const char* b) { return stricmp(a, b) < 0; };
	size_t suggestionsCount = 0;
	const char* suggestions[COMMAND_COMPLETION_MAXITEMS];

	for (const ConCommandBase* cmd = g_pCVar->GetCommands(); cmd; cmd = cmd->GetNext())
	{
		if (cmd->IsCommand())
			continue;  // Only care about actual ConVars

		const char* const cmdName = cmd->GetName();
		if (strnicmp(cmdName, partialParsed[1], partialCvarLength))
			continue;

		if (suggestionsCount < std::size(suggestions))
		{
			// We have spare room, don't worry about insertion sorting
			suggestions[suggestionsCount++] = cmdName;
		}
		else	// We're full, need to insertion sort, dropping off the last element
		{
			if (!suggestionsSorted)
			{
				// If we just finished filling up our array, the next command is going to need to
				// be insertion sorted. For that to work, we need the array sorted to begin with.
				std::sort(std::begin(suggestions), std::end(suggestions), suggestionsSorter);
				suggestionsSorted = true;
			}

			const auto& upperBound = std::upper_bound(std::begin(suggestions), std::end(suggestions), cmdName, suggestionsSorter);
			if (upperBound != std::end(suggestions))
			{
				// Shift elements afterwards to make room for new entry
				std::move(upperBound, std::end(suggestions) - 1, upperBound + 1);

				// Insert new entry
				*upperBound = cmdName;
			}
		}
	}

	// If we only ever encountered <= std::size(suggestions) possible suggestions, our array
	// will not be sorted. Do it now.
	if (!suggestionsSorted)
	{
		std::sort(std::begin(suggestions), std::begin(suggestions) + suggestionsCount, suggestionsSorter);
		suggestionsSorted = true;
	}

	// Copy suggestions into output array
	for (size_t i = 0; i < suggestionsCount; i++)
	{
		strcpy_s(commands[i], partialParsed[0]);
		commands[i][cmdNameLength] = ' ';
		commands[i][cmdNameLength + 1] = '\0';

		strncat(commands[i], suggestions[i], COMMAND_COMPLETION_ITEM_LENGTH - (cmdNameLength + 2));
	}

	return (int)suggestionsCount;
}

int ConsoleTools::FlagModifyAutocomplete(const char* partial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH])
{
	// Save original pointer
	const char* const flagModifyCmdName = partial;

	// Skip any leading whitespace
	while (*partial && isspace(*partial))
		partial++;

	// Determine length of command, including first space
	while (*partial)
	{
		partial++;

		if (isspace(*partial))
		{
			partial++;
			break;
		}
	}

	// Record command length
	const size_t cmdLengthWithSpace = partial - flagModifyCmdName;

	// Skip any extra whitespace between command and first argument
	while (*partial && isspace(*partial))
		partial++;

	// Number of characters after command name + 1 whitespace
	const auto partialLength = strlen(partial);

	bool suggestionsSorted = false;
	static const auto suggestionsSorter = [](const char* a, const char* b) { return stricmp(a, b) < 0; };
	size_t suggestionsCount = 0;
	const char* suggestions[COMMAND_COMPLETION_MAXITEMS];

	for (const ConCommandBase* cmd = g_pCVar->GetCommands(); cmd; cmd = cmd->GetNext())
	{
		const char* const cmdName = cmd->GetName();
		if (strnicmp(cmdName, partial, partialLength))
			continue;

		if (suggestionsCount < std::size(suggestions))
		{
			// We have spare room, don't worry about insertion sorting
			suggestions[suggestionsCount++] = cmdName;
		}
		else	// We're full, need to insertion sort, dropping off the last element
		{
			if (!suggestionsSorted)
			{
				// If we just finished filling up our array, the next command is going to need to
				// be insertion sorted. For that to work, we need the array sorted to begin with.
				std::sort(std::begin(suggestions), std::end(suggestions), suggestionsSorter);
				suggestionsSorted = true;
			}

			const auto& upperBound = std::upper_bound(std::begin(suggestions), std::end(suggestions), cmdName, suggestionsSorter);
			if (upperBound != std::end(suggestions))
			{
				// Shift elements afterwards to make room for new entry
				std::move(upperBound, std::end(suggestions) - 1, upperBound + 1);

				// Insert new entry
				*upperBound = cmdName;
			}
		}
	}

	// If we only ever encountered <= std::size(suggestions) possible suggestions, our array
	// will not be sorted. Do it now.
	if (!suggestionsSorted)
	{
		std::sort(std::begin(suggestions), std::begin(suggestions) + suggestionsCount, suggestionsSorter);
		suggestionsSorted = true;
	}

	// Copy suggestions into output array
	for (size_t i = 0; i < suggestionsCount; i++)
	{
		strncpy_s(commands[i], flagModifyCmdName, cmdLengthWithSpace);
		strncat(commands[i], suggestions[i], COMMAND_COMPLETION_ITEM_LENGTH - cmdLengthWithSpace - 1);
	}

	return (int)suggestionsCount;
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

int ConsoleTools::RemoveAliasAutocomplete(const char* partial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH])
{
	// Save original pointer
	const char* const flagModifyCmdName = partial;

	// Skip any leading whitespace
	while (*partial && isspace(*partial))
		partial++;

	// Determine length of command, including first space
	while (*partial)
	{
		partial++;

		if (isspace(*partial))
		{
			partial++;
			break;
		}
	}

	// Record command length
	const size_t cmdLengthWithSpace = partial - flagModifyCmdName;

	// Skip any extra whitespace between command and first argument
	while (*partial && isspace(*partial))
		partial++;

	// Number of characters after command name + 1 whitespace
	const auto partialLength = strlen(partial);

	std::vector<const char*> aliases;
	for (auto alias = *Interfaces::GetCmdAliases(); alias; alias = alias->next)
	{
		if (strnicmp(alias->name, partial, partialLength))
			continue;

		aliases.push_back(alias->name);
	}

	std::sort(aliases.begin(), aliases.end(), [](const char* a, const char* b) { return stricmp(a, b) < 0; });

	const size_t count = std::min<size_t>(COMMAND_COMPLETION_MAXITEMS, aliases.size());
	for (size_t i = 0; i < count; i++)
	{
		strncpy_s(commands[i], flagModifyCmdName, cmdLengthWithSpace);
		strncat(commands[i], aliases[i], COMMAND_COMPLETION_ITEM_LENGTH - cmdLengthWithSpace - 1);
	}

	return count;
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