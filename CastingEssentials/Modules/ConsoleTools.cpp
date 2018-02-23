#include "ConsoleTools.h"
#include "PluginBase/HookManager.h"

#include <vprof.h>

#include <regex>

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
	virtual ~PauseFilter()
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

	ce_consoletools_flags_add("ce_consoletools_flags_add", [](const CCommand& cmd) { GetModule()->AddFlags(cmd); }, "Adds a flag to a cvar."),
	ce_consoletools_flags_remove("ce_consoletools_flags_remove", [](const CCommand& cmd) { GetModule()->RemoveFlags(cmd); }, "Removes a flag from a cvar.")
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

	if (!GetHooks()->GetHook<HookManager::ICvar_ConsoleColorPrintf>())
	{
		PluginWarning("Required hook ICvar::ConsoleColorPrintf for module %s not available!\n", GetModuleName());
		ready = false;
	}
	if (!GetHooks()->GetHook<HookManager::ICvar_ConsoleDPrintf>())
	{
		PluginWarning("Required hook ICvar::ConsoleDPrintf for module %s not available!\n", GetModuleName());
		ready = false;
	}
	if (!GetHooks()->GetHook<HookManager::ICvar_ConsolePrintf>())
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
		const std::string filter = command.Arg(1);

		if (!m_Filters.count(filter))
			m_Filters.insert(filter);
		else
		{
			PauseFilter pause;
			PluginWarning("Filter %s is already present.\n", command.Arg(1));
		}
	}
	else
		PluginWarning("Usage: statusspec_consoletools_filter_add <filter>\n");
}
void ConsoleTools::ToggleFilterEnabled(IConVar *var, const char *pOldValue, float flOldValue)
{
	if (ce_consoletools_filter_enabled.GetBool())
	{
		if (!m_ConsolePrintfHook)
			m_ConsolePrintfHook = GetHooks()->AddHook<ICvar_ConsolePrintf>(std::bind(&ConsoleTools::ConsolePrintfHook, this, std::placeholders::_1));

		if (!m_ConsoleDPrintfHook)
			m_ConsoleDPrintfHook = GetHooks()->AddHook<ICvar_ConsoleDPrintf>(std::bind(&ConsoleTools::ConsoleDPrintfHook, this, std::placeholders::_1));

		if (!m_ConsoleColorPrintfHook)
			m_ConsoleColorPrintfHook = GetHooks()->AddHook<ICvar_ConsoleColorPrintf>(std::bind(&ConsoleTools::ConsoleColorPrintfHook, this, std::placeholders::_1, std::placeholders::_2));
	}
	else
		DisableHooks();
}

void ConsoleTools::DisableHooks()
{
	if (m_ConsoleColorPrintfHook && GetHooks()->RemoveHook<ICvar_ConsoleColorPrintf>(m_ConsoleColorPrintfHook, __FUNCSIG__))
	{
		m_ConsoleColorPrintfHook = 0;
	}
	if (m_ConsoleDPrintfHook && GetHooks()->RemoveHook<ICvar_ConsoleDPrintf>(m_ConsoleDPrintfHook, __FUNCSIG__))
	{
		m_ConsoleDPrintfHook = 0;
	}
	if (m_ConsolePrintfHook && GetHooks()->RemoveHook<ICvar_ConsolePrintf>(m_ConsolePrintfHook, __FUNCSIG__))
	{
		m_ConsolePrintfHook = 0;
	}
}

void ConsoleTools::ConsoleColorPrintfHook(const Color &clr, const char *message)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (!m_FilterPaused && CheckFilters(message))
		GetHooks()->GetHook<ICvar_ConsoleColorPrintf>()->SetState(Hooking::HookAction::SUPERCEDE);
}

void ConsoleTools::ConsoleDPrintfHook(const char *message)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (!m_FilterPaused && CheckFilters(message))
		GetHooks()->GetHook<ICvar_ConsoleDPrintf>()->SetState(Hooking::HookAction::SUPERCEDE);
}

void ConsoleTools::ConsolePrintfHook(const char *message)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (!m_FilterPaused && CheckFilters(message))
		GetHooks()->GetHook<ICvar_ConsolePrintf>()->SetState(Hooking::HookAction::SUPERCEDE);
}

bool ConsoleTools::CheckFilters(const std::string& message) const
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	for (std::string filter : m_Filters)
	{
		if (std::regex_search(message, std::regex(filter)))
			return true;
	}

	return false;
}

void ConsoleTools::RemoveFilter(const CCommand &command)
{
	if (command.ArgC() == 2)
	{
		const std::string filter = command.Arg(1);

		if (m_Filters.count(filter))
			m_Filters.erase(filter);
		else
			PluginWarning("Filter %s is not already present.\n", command.Arg(1));
	}
	else
		PluginWarning("Usage: statusspec_consoletools_filter_remove <filter>\n");
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

		if (!stricmp(flag, "game"))
			retVal |= FCVAR_GAMEDLL;
		else if (!stricmp(flag, "client"))
			retVal |= FCVAR_CLIENTDLL;
		else if (!stricmp(flag, "archive"))
			retVal |= FCVAR_ARCHIVE;
		else if (!stricmp(flag, "notify"))
			retVal |= FCVAR_NOTIFY;
		else if (!stricmp(flag, "singleplayer"))
			retVal |= FCVAR_SPONLY;
		else if (!stricmp(flag, "notconnected"))
			retVal |= FCVAR_NOT_CONNECTED;
		else if (!stricmp(flag, "cheat"))
			retVal |= FCVAR_CHEAT;
		else if (!stricmp(flag, "replicated"))
			retVal |= FCVAR_REPLICATED;
		else if (!stricmp(flag, "server_can_execute"))
			retVal |= FCVAR_SERVER_CAN_EXECUTE;
		else if (!stricmp(flag, "clientcmd_can_execute"))
			retVal |= FCVAR_CLIENTCMD_CAN_EXECUTE;
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
		ss << "     " << filter << '\n';

	{
		PauseFilter pause;
		PluginMsg(ss.str().c_str());
	}
}