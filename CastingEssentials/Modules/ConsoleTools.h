#pragma once
#include "PluginBase/Modules.h"
#include <set>

class CCommand;
class ConCommand;
class ConVar;
class IConVar;

class ConsoleTools final : public Module
{
public:
	ConsoleTools();
	virtual ~ConsoleTools();

	static bool CheckDependencies();
	static ConsoleTools* GetModule() { return Modules().GetModule<ConsoleTools>(); }

private:
	void ConsoleColorPrintfHook(const Color& color, const char* msg);
	void ConsoleDPrintfHook(const char* msg);
	void ConsolePrintfHook(const char* msg);

	bool CheckFilters(const std::string& msg) const;

	int m_ConsoleColorPrintfHook;
	int m_ConsoleDPrintfHook;
	int m_ConsolePrintfHook;
	std::set<std::string> m_Filters;

	ConVar* m_FilterEnabled;
	ConCommand* m_FilterAdd;
	ConCommand* m_FilterRemove;
	ConCommand* m_FilterList;

	ConCommand* m_FlagsAdd;
	ConCommand* m_FlagsRemove;

	static void StaticAddFilter(const CCommand& command);
	void AddFilter(const CCommand& command);
	static void StaticRemoveFilter(const CCommand& command);
	void RemoveFilter(const CCommand& command);
	static void StaticListFilter(const CCommand& command);
	void ListFilters(const CCommand& command);
	static void StaticToggleFilterEnabled(IConVar* var, const char* oldValue, float fOldValue);
	void ToggleFilterEnabled(IConVar* var, const char* oldValue, float fOldValue);

	static void StaticAddFlags(const CCommand& command);
	void AddFlags(const CCommand& command);
	static void StaticRemoveFlags(const CCommand& command);
	void RemoveFlags(const CCommand& command);

	class PauseFilter;
	int m_FilterPaused;

	static int ParseFlags(const CCommand& command);
};