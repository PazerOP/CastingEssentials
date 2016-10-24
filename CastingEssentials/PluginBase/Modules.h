#pragma once

#include <map>
#include <string>
#include <typeindex>

#include "common.h"
#include "exceptions.h"

class Module
{
public:
	virtual ~Module() = default;

	static bool CheckDependencies() { return true; };
};

class ModuleManager
{
public:
	template <typename ModuleType> ModuleType *GetModule() const;
	template <typename ModuleType> const std::string& GetModuleName() const;
	template <typename ModuleType> bool RegisterAndLoadModule(const std::string& moduleName);
	void UnloadAllModules();
private:
	std::map<std::type_index, Module *> modules;
	std::map<std::type_index, std::string> moduleNames;
};

template <typename ModuleType> inline ModuleType *ModuleManager::GetModule() const
{
	auto found = modules.find(typeid(ModuleType));
	if (found != modules.end())
		return static_cast<ModuleType *>(found->second);
	else
		throw module_not_loaded(GetModuleName<ModuleType>().c_str());
}

template <typename ModuleType> inline const std::string& ModuleManager::GetModuleName() const
{
	auto found = moduleNames.find(typeid(ModuleType));
	if (found != moduleNames.end())
		return found->second;
	else
	{
		static std::string s_UnknownString = "[Unknown]";
		return s_UnknownString;
	}
}

template <typename ModuleType> inline bool ModuleManager::RegisterAndLoadModule(const std::string& moduleName)
{
	moduleNames[typeid(ModuleType)] = moduleName;

	if (ModuleType::CheckDependencies())
	{
		modules[typeid(ModuleType)] = new ModuleType();

		PluginColorMsg(Color(0, 255, 0, 255), "Module %s loaded successfully!\n", moduleName.c_str());
		return true;
	}
	else
	{
		PluginColorMsg(Color(255, 0, 0, 255), "Module %s failed to load!\n", moduleName.c_str());
		return false;
	}
}

inline void ModuleManager::UnloadAllModules()
{
	for (auto iterator : modules)
	{
		PluginColorMsg(Color(0, 255, 0, 255), "Module %s unloaded!\n", moduleNames[iterator.first].c_str());

		delete iterator.second;
		iterator.second = nullptr;
	}

	modules.clear();
}

extern ModuleManager& Modules();