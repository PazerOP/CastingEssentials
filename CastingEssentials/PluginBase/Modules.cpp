#include "Modules.h"
#include "PluginBase/Interfaces.h"
#include "Controls/StubPanel.h"

#include <cdll_int.h>
#include <vprof.h>

static ModuleManager s_ModuleManager;
ModuleManager& Modules() { return s_ModuleManager; }

bool IBaseModule::s_InGame;
ModuleDesc* g_ModuleList = nullptr;

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
		auto mod = iterator->m_Module->ReplaceSingleton(nullptr); // Grab the singleton instance
		const std::string moduleName(mod->GetModuleName());
		mod = nullptr;
		PluginColorMsg(Color(0, 255, 0, 255), "Module %s unloaded!\n", moduleName.c_str());
	}

	modules.clear();
	m_Panel.reset();
}

void ModuleManager::LoadAll()
{
	auto md = g_ModuleList;
	while (md) {
		try {
			Load(*md);
		}
		catch (const std::exception&) { }
		md = md->next;
	}
}

void ModuleManager::Load(ModuleDesc& desc)
{
	switch (desc.state) {
	case ModuleState::MODULE_FAILED:
		throw module_load_failed(desc.name.c_str());
	case ModuleState::MODULE_LOADING:
		throw module_circular_dependency(desc.name.c_str());
	case ModuleState::MODULE_LOADED:
		return;
	}

	desc.state = ModuleState::MODULE_LOADING;
	try
	{
		auto mod = desc.factory();
		Assert(mod);
		modules.push_back({ mod.get() });
		mod->ReplaceSingleton(std::move(mod));
		desc.state = ModuleState::MODULE_LOADED;
		PluginColorMsg(Color(0, 255, 0, 255), "Module %s loaded successfully!\n", desc.name.c_str());
	}
	catch (const module_circular_dependency& e)
	{
		desc.state = ModuleState::MODULE_FAILED;
		PluginColorMsg(Color(255, 0, 0, 255), "Module %s failed to load because of circular dependency on module %s!\n", desc.name.c_str(), e.what());
		throw module_load_failed(desc.name.c_str());
	}
	catch (const module_dependency_failed& e)
	{
		desc.state = ModuleState::MODULE_FAILED;
		PluginColorMsg(Color(255, 0, 0, 255), "Module %s failed to load because dependency %s did not load!\n", desc.name.c_str(), e.what());
		throw module_load_failed(desc.name.c_str());
	}
	catch (const std::exception&)
	{
		desc.state = ModuleState::MODULE_FAILED;
		PluginColorMsg(Color(255, 0, 0, 255), "Module %s failed to load!\n", desc.name.c_str());
		throw module_load_failed(desc.name.c_str());
	}
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
	auto ite = modules.begin();
	while (ite != modules.end()) {
		try {
			ite->m_Module->OnTick(inGame);
			++ite;
		}
		catch (std::exception e) {
			PluginColorMsg(Color(255, 0, 0, 255), "Module %s tick failed, disabling: %s\n", ite->m_Module->GetModuleName(), e.what());
			ite = modules.erase(ite);
		}
	}
}