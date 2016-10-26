#include "ConsoleTools.h"
#include "PluginBase/Funcs.h"

#include <convar.h>
#include <regex>

class ConsoleTools::PauseFilter final
{
public:
	PauseFilter()
	{
		ConsoleTools* module = Modules().GetModule<ConsoleTools>();
		if (!module)
			return;

		module->m_FilterPaused++;
	}
	virtual ~PauseFilter()
	{
		ConsoleTools* module = Modules().GetModule<ConsoleTools>();
		if (!module)
			return;

		module->m_FilterPaused--;
		Assert(module->m_FilterPaused >= 0);
	}
};

ConsoleTools::ConsoleTools()
{
	m_FilterPaused = false;
	m_ConsoleColorPrintfHook = 0;
	m_ConsoleDPrintfHook = 0;
	m_ConsolePrintfHook = 0;

	m_FilterEnabled = new ConVar("ce_consoletools_filter_enabled", "0", FCVAR_NONE, "Enables or disables console filtering.", &ConsoleTools::StaticToggleFilterEnabled);
	m_FilterAdd = new ConCommand("ce_consoletools_filter_add", &ConsoleTools::StaticAddFilter, "Adds a new console filter. Uses regular expressions.");
	m_FilterRemove = new ConCommand("ce_consoletools_filter_remove", &ConsoleTools::StaticRemoveFilter, "Removes an existing console filter.");
	m_FilterList = new ConCommand("ce_consoletools_filter_list", &ConsoleTools::StaticListFilter, "Lists all console filters.");

	m_FlagsAdd = new ConCommand("ce_consoletools_flags_add", &ConsoleTools::StaticAddFlags, "Adds a flag to a cvar.");
	m_FlagsRemove = new ConCommand("ce_consoletools_flags_remove", &ConsoleTools::StaticRemoveFlags, "Removes a flag from a cvar.");
}
ConsoleTools::~ConsoleTools()
{
	if (m_ConsoleColorPrintfHook)
	{
		Funcs::RemoveHook(m_ConsoleColorPrintfHook, __FUNCSIG__);
		m_ConsoleColorPrintfHook = 0;
	}
	if (m_ConsoleDPrintfHook)
	{
		Funcs::RemoveHook(m_ConsoleDPrintfHook, __FUNCSIG__);
		m_ConsoleDPrintfHook = 0;
	}
	if (m_ConsolePrintfHook)
	{
		Funcs::RemoveHook(m_ConsolePrintfHook, __FUNCSIG__);
		m_ConsolePrintfHook = 0;
	}
}

void ConsoleTools::StaticAddFilter(const CCommand& command)
{
	ConsoleTools* module = Modules().GetModule<ConsoleTools>();
	if (!module)
	{
		PluginWarning("Unable to use %s: module not loaded\n", command.Arg(0));
		return;
	}

	module->AddFilter(command);
}
void ConsoleTools::StaticRemoveFilter(const CCommand& command)
{
	ConsoleTools* module = Modules().GetModule<ConsoleTools>();
	if (!module)
	{
		PluginWarning("Unable to use %s: module not loaded\n", command.Arg(0));
		return;
	}

	module->RemoveFilter(command);
}
void ConsoleTools::StaticListFilter(const CCommand& command)
{
	ConsoleTools* module = Modules().GetModule<ConsoleTools>();
	if (!module)
	{
		PluginWarning("Unable to use %s: module not loaded\n", command.Arg(0));
		return;
	}

	module->ListFilters(command);
}
void ConsoleTools::StaticToggleFilterEnabled(IConVar* var, const char* oldValue, float fOldValue)
{
	ConsoleTools* module = Modules().GetModule<ConsoleTools>();
	if (!module)
	{
		PluginWarning("Unable to use %s: module not loaded\n", var->GetName());
		return;
	}

	module->ToggleFilterEnabled(var, oldValue, fOldValue);
}
void ConsoleTools::StaticAddFlags(const CCommand& command)
{
	ConsoleTools* module = Modules().GetModule<ConsoleTools>();
	if (!module)
	{
		PluginWarning("Unable to use %s: module not loaded\n", command.Arg(0));
		return;
	}

	module->AddFlags(command);
}
void ConsoleTools::StaticRemoveFlags(const CCommand& command)
{
	ConsoleTools* module = Modules().GetModule<ConsoleTools>();
	if (!module)
	{
		PluginWarning("Unable to use %s: module not loaded\n", command.Arg(0));
		return;
	}

	module->RemoveFlags(command);
}

bool ConsoleTools::CheckDependencies()
{
	bool ready = true;

	if (!g_pCVar)
	{
		PluginWarning("Required interface ICVar for module %s not available!\n", Modules().GetModuleName<ConsoleTools>().c_str());
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
	if (m_FilterEnabled->GetBool())
	{
		if (!m_ConsolePrintfHook)
			m_ConsolePrintfHook = Funcs::AddHook_ICvar_ConsolePrintf(g_pCVar, std::bind(&ConsoleTools::ConsolePrintfHook, this, std::placeholders::_1));

		if (!m_ConsoleDPrintfHook)
			m_ConsoleDPrintfHook = Funcs::AddHook_ICvar_ConsoleDPrintf(g_pCVar, std::bind(&ConsoleTools::ConsoleDPrintfHook, this, std::placeholders::_1));

		if (!m_ConsoleColorPrintfHook)
			m_ConsoleColorPrintfHook = Funcs::s_Hook_ICvar_ConsoleColorPrintf->AddHook(std::bind(&ConsoleTools::ConsoleColorPrintfHook, this, std::placeholders::_1, std::placeholders::_2));
	}
	else
	{
		if (m_ConsoleColorPrintfHook)
		{
			if (Funcs::RemoveHook(m_ConsoleColorPrintfHook, __FUNCSIG__))
				m_ConsoleColorPrintfHook = 0;
		}

		if (m_ConsoleDPrintfHook)
		{
			if (Funcs::RemoveHook(m_ConsoleDPrintfHook, __FUNCSIG__))
				m_ConsoleDPrintfHook = 0;
		}

		if (m_ConsolePrintfHook)
		{
			if (Funcs::RemoveHook(m_ConsolePrintfHook, __FUNCSIG__))
				m_ConsolePrintfHook = 0;
		}
	}
}

void ConsoleTools::ConsoleColorPrintfHook(const Color &clr, const char *message)
{
	if (!m_FilterPaused && CheckFilters(message))
		return;

	Funcs::s_Hook_ICvar_ConsoleColorPrintf->GetOriginal()(clr, message);
}

void ConsoleTools::ConsoleDPrintfHook(const char *message)
{
	if (!m_FilterPaused && CheckFilters(message))
		return;

	Funcs::Original_ICvar_ConsoleDPrintf()(message);
}

void ConsoleTools::ConsolePrintfHook(const char *message)
{
	if (!m_FilterPaused && CheckFilters(message))
		return;

	Funcs::Original_ICvar_ConsolePrintf()(message);
}

bool ConsoleTools::CheckFilters(const std::string& message) const
{
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
		PluginWarning("Usage: statusspec_consoletools_flags_remove <name> <flag1> [flag2 ...]\n");
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
		PluginWarning("Usage: statusspec_consoletools_flags_add <name> <flag1> [flag2 ...]\n");
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