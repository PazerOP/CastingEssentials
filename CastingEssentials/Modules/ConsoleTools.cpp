#include "ConsoleTools.h"
#include "PluginBase/Funcs.h"

#include <convar.h>
#include <sourcehook.h>
#include <regex>

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

ConsoleTools::ConsoleTools()
{
	m_ConsoleColorPrintfHook = 0;
	m_ConsoleDPrintfHook = 0;
	m_ConsolePrintfHook = 0;

	m_FilterEnabled = new ConVar("ce_consoletools_filter_enabled", "0", FCVAR_NONE, "Enables or disables console filtering.", [](IConVar* var, const char* oldValue, float fOldValue) { Modules().GetModule<ConsoleTools>()->ToggleFilterEnabled(var, oldValue, fOldValue); });
	m_FilterAdd = new ConCommand("ce_consoletools_filter_add", &ConsoleTools::StaticAddFilter, "Adds a new console filter. Uses regular expressions.", FCVAR_NONE);
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
			PluginWarning("Filter %s is already present.\n", command.Arg(1));
	}
	else
		PluginWarning("Usage: statusspec_consoletools_filter_add <filter>\n");
}
void ConsoleTools::ToggleFilterEnabled(IConVar *var, const char *pOldValue, float flOldValue)
{
	if (m_FilterEnabled->GetBool())
	{
		if (!m_ConsoleColorPrintfHook)
			m_ConsoleColorPrintfHook = Funcs::AddHook_ICvar_ConsoleColorPrintf(g_pCVar, SH_MEMBER(this, &ConsoleTools::ConsoleColorPrintfHook), false);

		if (!m_ConsoleDPrintfHook)
			m_ConsoleDPrintfHook = Funcs::AddHook_ICvar_ConsoleDPrintf(g_pCVar, SH_MEMBER(this, &ConsoleTools::ConsoleDPrintfHook), false);

		if (!m_ConsolePrintfHook)
			m_ConsolePrintfHook = Funcs::AddHook_ICvar_ConsolePrintf(g_pCVar, SH_MEMBER(this, &ConsoleTools::ConsolePrintfHook), false);
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
	if (CheckFilters(message))
	{
		RETURN_META(MRES_SUPERCEDE);
	}

	RETURN_META(MRES_IGNORED);
}

void ConsoleTools::ConsoleDPrintfHook(const char *message)
{
	if (CheckFilters(message))
	{
		RETURN_META(MRES_SUPERCEDE);
	}

	RETURN_META(MRES_IGNORED);
}

void ConsoleTools::ConsolePrintfHook(const char *message)
{
	if (CheckFilters(message))
	{
		RETURN_META(MRES_SUPERCEDE);
	}

	RETURN_META(MRES_IGNORED);
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