#pragma once

#include <vector>
#include <memory>
#include <string>
#include <typeindex>
#include <functional>

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
	virtual std::unique_ptr<IBaseModule> ReplaceSingleton(std::unique_ptr<IBaseModule> replacement) = 0; // Replaces global singleton and returns the old instance

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
	virtual std::unique_ptr<IBaseModule> ReplaceSingleton(std::unique_ptr<IBaseModule> replacement) override {
		replacement.swap(s_Module);
		return replacement;
	}

	static std::unique_ptr<IBaseModule> s_Module;
};

template<class T> std::unique_ptr<IBaseModule> Module<T>::s_Module = nullptr;	// Static module pointer variable definition

struct ModuleDesc;
// A global list of all known modules
extern ModuleDesc* g_ModuleList;

enum class ModuleState
{
	MODULE_UNLOADED,
	MODULE_LOADING,
	MODULE_FAILED,
	MODULE_LOADED,
};

struct ModuleDesc
{
	const std::type_info& ti;
	std::function<std::unique_ptr<IBaseModule>()> factory;
	const std::string name;
	ModuleState state = ModuleState::MODULE_UNLOADED;
	ModuleDesc* next;

	ModuleDesc(const std::type_info& ti, const char* name, std::function<std::unique_ptr<IBaseModule>()> factory) : ti(ti), name(name), factory(factory) {
		this->next = g_ModuleList;
		g_ModuleList = this;
	}
};

#define MODULE_REGISTER(T) ModuleDesc module_desc_ ## T (typeid(T), T::GetModuleName(), []() -> std::unique_ptr<IBaseModule> { \
	if (!T::CheckDependencies()) throw module_load_failed(T::GetModuleName()); \
	return std::make_unique<T>(); \
});

class ModuleManager final
{
public:
	void Init();
	void UnloadAllModules();

	template <typename ModuleType> ModuleType *GetModule() const;
	template <typename ModuleType> constexpr const char* GetModuleName() const;
	template <typename ModuleType> void Depend();
	void LoadAll();

	std::size_t size() { return modules.size(); }

private:

	void Load(ModuleDesc& desc);

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

template <typename ModuleType> inline void ModuleManager::Depend()
{
	// Find the module we need in the global list
	const auto& ti = typeid(ModuleType);
	auto md = g_ModuleList;
	while (md) {
		if (md->ti == ti) {
			try {
				Load(*md);
			}
			catch (const std::exception&) {
				throw module_dependency_failed(md->name.c_str());
			}
			return;
		}
		md = md->next;
	}
	throw module_load_failed(ti.name());
}

extern ModuleManager& Modules();