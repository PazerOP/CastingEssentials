#include "ConsoleTools.h"

#include "Misc/CmdAlias.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"

#include <vprof.h>

#include <regex>

#undef min

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
		[](IConVar* var, const char* oldValue, float fOldValue) { GetModule()->ToggleFilterEnabled(var, oldValue, fOldValue); }),
	ce_consoletools_filter_add("ce_consoletools_filter_add", [](const CCommand& cmd) { GetModule()->AddFilter(cmd); },
		"Adds a new console filter. Uses regular expressions."),
	ce_consoletools_filter_remove("ce_consoletools_filter_remove", [](const CCommand& cmd) { GetModule()->RemoveFilter(cmd); },
		"Removes an existing console filter."),
	ce_consoletools_filter_list("ce_consoletools_filter_list", [](const CCommand& cmd) { GetModule()->ListFilters(cmd); }, "Lists all console filters."),

	ce_consoletools_flags_add("ce_consoletools_flags_add", [](const CCommand& cmd) { GetModule()->AddFlags(cmd); },
		"Adds a flag to a cvar.", FCVAR_NONE, FlagModifyAutocomplete),
	ce_consoletools_flags_remove("ce_consoletools_flags_remove", [](const CCommand& cmd) { GetModule()->RemoveFlags(cmd); },
		"Removes a flag from a cvar.", FCVAR_NONE, FlagModifyAutocomplete),

	ce_consoletools_alias_remove("ce_consoletools_alias_remove", RemoveAlias,
		"Removes an existing alias created with the \"alias\" command.", FCVAR_NONE, RemoveAliasAutocomplete)
{
	m_FilterPaused = false;
	m_ConsoleColorPrintfHook = 0;
	m_ConsoleDPrintfHook = 0;
	m_ConsolePrintfHook = 0;
}
ConsoleTools::~ConsoleTools()
{
	DisableHooks();
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
void ConsoleTools::ToggleFilterEnabled(IConVar *var, const char *pOldValue, float flOldValue)
{
	if (ce_consoletools_filter_enabled.GetBool())
	{
		if (!m_ConsolePrintfHook)
			m_ConsolePrintfHook = GetHooks()->AddHook<HookFunc::ICvar_ConsolePrintf>(std::bind(&ConsoleTools::ConsolePrintfHook, this, std::placeholders::_1));

		if (!m_ConsoleDPrintfHook)
			m_ConsoleDPrintfHook = GetHooks()->AddHook<HookFunc::ICvar_ConsoleDPrintf>(std::bind(&ConsoleTools::ConsoleDPrintfHook, this, std::placeholders::_1));

		if (!m_ConsoleColorPrintfHook)
			m_ConsoleColorPrintfHook = GetHooks()->AddHook<HookFunc::ICvar_ConsoleColorPrintf>(std::bind(&ConsoleTools::ConsoleColorPrintfHook, this, std::placeholders::_1, std::placeholders::_2));
	}
	else
		DisableHooks();
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

void ConsoleTools::DisableHooks()
{
	if (m_ConsoleColorPrintfHook && GetHooks()->RemoveHook<HookFunc::ICvar_ConsoleColorPrintf>(m_ConsoleColorPrintfHook, __FUNCSIG__))
	{
		m_ConsoleColorPrintfHook = 0;
	}
	if (m_ConsoleDPrintfHook && GetHooks()->RemoveHook<HookFunc::ICvar_ConsoleDPrintf>(m_ConsoleDPrintfHook, __FUNCSIG__))
	{
		m_ConsoleDPrintfHook = 0;
	}
	if (m_ConsolePrintfHook && GetHooks()->RemoveHook<HookFunc::ICvar_ConsolePrintf>(m_ConsolePrintfHook, __FUNCSIG__))
	{
		m_ConsolePrintfHook = 0;
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

// this is a total hijack of a friend class declared but never defined in the public SDK
class CCvar
{
public:
	static int GetFlags(ConCommandBase *base) { return base->m_nFlags; }
	static void SetFlags(ConCommandBase *base, int flags) { base->m_nFlags = flags; }
};

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