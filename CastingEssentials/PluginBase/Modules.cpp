#include "Modules.h"
#include "PluginBase/Interfaces.h"
#include "Controls/StubPanel.h"

#include <cdll_int.h>
#include <vprof.h>

static ModuleManager s_ModuleManager;
ModuleManager& Modules() { return s_ModuleManager; }

bool IBaseModule::s_InGame;

class ModuleManager::Panel final : public vgui::StubPanel
{
public:
	void OnTick() override;

private:
	void LevelInitAllModules();
	void LevelShutdownAllModules();

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
		auto mod = iterator->m_Module->ZeroSingleton(); // Grab the singleton instance
		const std::string moduleName(mod->GetModuleName());
		mod = nullptr;
		PluginColorMsg(Color(0, 255, 0, 255), "Module %s unloaded!\n", moduleName.c_str());
	}

	modules.clear();
	m_Panel.reset();
}

void ModuleManager::Panel::LevelInitAllModules()
{
	IBaseModule::s_InGame = true;

	for (const auto& data : Modules().modules)
		data.m_Module->LevelInit();
}
void ModuleManager::Panel::LevelShutdownAllModules()
{
	IBaseModule::s_InGame = false;

	for (const auto& data : Modules().modules)
		data.m_Module->LevelShutdown();
}

void ModuleManager::Panel::OnTick()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);

	const bool inGame = Interfaces::GetEngineClient()->IsInGame();

	if (inGame)
	{
		const char* const levelName = Interfaces::GetEngineClient()->GetLevelName();
		if (stricmp(m_LastLevelName.c_str(), levelName))
		{
			if (!m_LastLevelName.empty())
				LevelShutdownAllModules();

			m_LastLevelName = levelName;

			LevelInitAllModules();
		}
	}
	else if (!m_LastLevelName.empty())
	{
		LevelShutdownAllModules();

		m_LastLevelName.clear();
	}

	Modules().TickAllModules(inGame);
}

void ModuleManager::TickAllModules(bool inGame)
{
	for (const auto& data : modules)
		data.m_Module->OnTick(inGame);
}