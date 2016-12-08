#include "Modules.h"
#include "PluginBase/Interfaces.h"
#include "Controls/StubPanel.h"

#include <cdll_int.h>
#include <vprof.h>

static ModuleManager s_ModuleManager;
ModuleManager& Modules() { return s_ModuleManager; }

class ModuleManager::Panel final : public vgui::StubPanel
{
public:
	void OnTick() override;

private:
	std::string m_LastLevelName;
};

void ModuleManager::Init()
{
	m_Panel.reset(new Panel());
}

void ModuleManager::UnloadAllModules()
{
	for (auto iterator = modules.rbegin(); iterator != modules.rend(); iterator++)
	{
		iterator->second.reset();
		PluginColorMsg(Color(0, 255, 0, 255), "Module %s unloaded!\n", moduleNames[iterator->first].c_str());
	}

	modules.clear();
	m_Panel.reset();
}

void ModuleManager::Panel::OnTick()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);

	const bool inGame = Interfaces::GetEngineClient()->IsInGame();
	Modules().TickAllModules(inGame);

	if (inGame)
	{
		const char* const levelName = Interfaces::GetEngineClient()->GetLevelName();
		if (stricmp(m_LastLevelName.c_str(), levelName))
		{
			m_LastLevelName = levelName;
			for (const auto& pair : Modules().modules)
				pair.second->LevelInitPreEntity();
		}
	}
	else
	{
		m_LastLevelName.clear();
	}
}

void ModuleManager::TickAllModules(bool inGame)
{
	for (const auto& pair : modules)
		pair.second->OnTick(inGame);
}