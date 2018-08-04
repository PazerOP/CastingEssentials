#pragma once

#include <vector>
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

	virtual const char* GetModuleName() = 0; // ModuleManager gets to call this so it can retrieve module name after type erasure
	virtual std::unique_ptr<IBaseModule> ZeroSingleton() = 0; // Unset the global singleton and return the instance that used to be there

	static bool s_InGame;
};

template<class T>
class Module : public IBaseModule
{
public:
	virtual ~Module() = default;

	static __forceinline T* GetModule() {
		if (!s_Module) throw module_not_loaded(T::GetModuleName());
		return reinterpret_cast<T*>(s_Module.get());
	}

private:
	friend class ModuleManager;

	virtual const char* GetModuleName() override { return T::GetModuleName(); }
	virtual std::unique_ptr<IBaseModule> ZeroSingleton() override { return std::move(s_Module); }

	static std::unique_ptr<IBaseModule> s_Module;
};

template<class T> std::unique_ptr<IBaseModule> Module<T>::s_Module = nullptr;	// Static module pointer variable definition

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
		IBaseModule* m_Module;
	};

	std::vector<ModuleData> modules;
};

template <typename ModuleType> inline ModuleType *ModuleManager::GetModule() const
{
	return ModuleType::GetModule();
}

template <typename ModuleType> inline constexpr const char* ModuleManager::GetModuleName() const
{
	return ModuleType::GetModuleName();
}

template <typename ModuleType> inline bool ModuleManager::RegisterAndLoadModule()
{
	const auto moduleName = ModuleType::GetModuleName();
	if (ModuleType::s_Module) {
		PluginColorMsg(Color(0, 0, 255, 255), "Module %s already loaded!\n", moduleName);
		return true;
	}
	else if (ModuleType::CheckDependencies())
	{
		ModuleType::s_Module.reset(new ModuleType());
		modules.push_back({ ModuleType::s_Module.get() });

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