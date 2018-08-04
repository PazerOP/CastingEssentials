#pragma once

#include <map>
#include <memory>
#include <string>
#include <typeindex>

#include "exceptions.h"

class IBaseModule
{
public:
	virtual ~IBaseModule() = default;

	static bool CheckDependencies() { return true; }

protected:
	static bool IsInGame() { return s_InGame; }

	virtual void OnTick(bool inGame) { }
	virtual void LevelInit() { }
	virtual void LevelShutdown() { }

private:
	friend class ModuleManager;

	// ModuleManager gets to call this so it can retrieve module name after type erasure
	virtual const char* GetModuleName() = 0;

	static bool s_InGame;
};

template<class T>
class Module : public IBaseModule
{
public:
	virtual ~Module() = default;

	static __forceinline T * GetModule() { return s_Module; }

private:
	friend class ModuleManager;

	virtual const char* GetModuleName() override { return T::GetModuleName(); }

	static T* s_Module;
};

template<class T> T* Module<T>::s_Module = nullptr;	// Static module pointer variable definition

class ModuleManager final
{
public:
	void Init();
	void UnloadAllModules();

	template <typename ModuleType> ModuleType *GetModule() const;
	template <typename ModuleType> constexpr const char* GetModuleName() const;
	template <typename ModuleType> bool RegisterAndLoadModule();

private:
	class Panel;
	std::unique_ptr<Panel> m_Panel;

	void TickAllModules(bool inGame);

	struct ModuleData
	{
		std::unique_ptr<IBaseModule> m_Module;
		void** m_Pointer;
	};

	std::map<std::type_index, ModuleData> modules;
};

template <typename ModuleType> inline ModuleType *ModuleManager::GetModule() const
{
	static const std::type_index s_ThisModuleType = typeid(ModuleType);

	auto found = modules.find(s_ThisModuleType);
	if (found != modules.end())
		return static_cast<ModuleType *>(found->second.m_Module.get());
	else
		throw module_not_loaded(GetModuleName<ModuleType>().c_str());
}

template <typename ModuleType> inline constexpr const char* ModuleManager::GetModuleName() const
{
	return ModuleType::GetModuleName();
}

template <typename ModuleType> inline bool ModuleManager::RegisterAndLoadModule()
{
	const auto moduleName = ModuleType::GetModuleName();
	if (ModuleType::CheckDependencies())
	{
		{
			ModuleData& data = modules[typeid(ModuleType)];
			Assert(!data.m_Module);
			data.m_Module.reset(ModuleType::s_Module = new ModuleType());
			data.m_Pointer = reinterpret_cast<void**>(&ModuleType::s_Module);
		}

		PluginColorMsg(Color(0, 255, 0, 255), "Module %s loaded successfully!\n", moduleName);
		return true;
	}
	else
	{
		PluginColorMsg(Color(255, 0, 0, 255), "Module %s failed to load!\n", moduleName);
		return false;
	}
}

extern ModuleManager& Modules();