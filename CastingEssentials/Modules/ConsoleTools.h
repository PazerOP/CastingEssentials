#pragma once
#include "Misc/CommandCallbacks.h"
#include "PluginBase/Hook.h"
#include "PluginBase/Modules.h"

#include <convar.h>

#include <regex>
#include <unordered_map>

class ConsoleTools final : public Module<ConsoleTools>
{
public:
	ConsoleTools();

	static bool CheckDependencies();
	static constexpr __forceinline const char* GetModuleName() { return "Console Tools"; }

private:
	void ConsoleColorPrintfHook(const Color& color, const char* msg);
	void ConsoleDPrintfHook(const char* msg);
	void ConsolePrintfHook(const char* msg);

	bool CheckFilters(const char* msg) const;

	Hook<HookFunc::ICvar_ConsoleColorPrintf> m_ConsoleColorPrintfHook;
	Hook<HookFunc::ICvar_ConsoleDPrintf> m_ConsoleDPrintfHook;
	Hook<HookFunc::ICvar_ConsolePrintf> m_ConsolePrintfHook;

	std::unordered_map<std::string, std::regex> m_Filters;

	ConVar ce_consoletools_filter_enabled;
	ConCommand ce_consoletools_filter_add;
	ConCommand ce_consoletools_filter_remove;
	ConCommand ce_consoletools_filter_list;

	ConCommand ce_consoletools_flags_add;
	ConCommand ce_consoletools_flags_remove;

	ConCommand ce_consoletools_limits_set;

	ConCommand ce_consoletools_alias_remove;

	ConCommand ce_consoletools_unhide_all_cvars;
	static void UnhideAllCvars();

	void AddFilter(const CCommand& command);
	void RemoveFilter(const CCommand& command);
	void ListFilters(const CCommand& command);
	void ToggleFilterEnabled(const ConVar* var);

	CommandCallbacks m_SetLimitsCallbacks;
	static void SetLimits(const CCommand& command);
	static void SetLimitsAutocomplete(const CCommand& partial, CUtlVector<CUtlString>& suggestions);

	CommandCallbacks m_AddFlagsCallbacks;
	CommandCallbacks m_RemoveFlagsCallbacks;
	static void AddFlags(const CCommand& command);
	static void RemoveFlags(const CCommand& command);
	static void FlagModifyAutocomplete(const CCommand& partial, CUtlVector<CUtlString>& suggestions);

	CommandCallbacks m_RemoveAliasCallbacks;
	static void RemoveAlias(const CCommand& command);
	static void RemoveAliasAutocomplete(const CCommand& partial, CUtlVector<CUtlString>& suggestions);

	class PauseFilter;
	int m_FilterPaused;

	static int ParseFlags(const CCommand& command);
};