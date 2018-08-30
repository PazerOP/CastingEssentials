#pragma once
#pragma once

#include "PluginBase/Modules.h"
#include "PluginBase/Entities.h"
#include <map>

class SrcTVPlusListener;
class CRecvProxyData;

// Detects the presence of SrcTV+(or equivalent)
//
// Other modules can use this to figure out if they should show extra information based
// on the presence of SrcTV+ or an equivalent plugin. They should not declare a dependency
// on this module, but rather use the `IsAvailable()` and `SrcTVPlusListener`. The former
// allows checking if SrcTV+ is available, the latter allows listener for it becoming
// (un)available.
//
// All of the members related to interfacing with other modules is static. This is for both
// performance but also robustness. Should SrcTV+ detection fail for any reason, the static
// members will continue to work and act as if SrcTV+ is disabled.

class SrcTVPlus : public Module<SrcTVPlus>
{
public:
	__forceinline static bool IsAvailable() { return s_Detected; }
	using Callback = void(bool);

protected:

	template<typename F> static int register_listener(F&& cb) {
		auto id = s_MaxListener++;
		s_Listeners.emplace(id, std::forward<F>(cb));
		cb(s_Detected); // Call with current value
		return id;
	}
	static void unregister_listener(int id) {
		s_Listeners.erase(id);
	}
	friend class SrcTVPlusListener;

// Module implementation
public:
	static bool CheckDependencies();
	static constexpr __forceinline const char* GetModuleName() { return "SrcTV+ detection"; }

protected:
	// When entering/leaving a level we reset the detector
	virtual void LevelInit() { SetDetected(false, true); EnableDetector(); }
	virtual void LevelShutdown() { SetDetected(false); DisableDetector(); }

	static void EnableDetector();
	static void DisableDetector();
	static void DetectorProxy(const CRecvProxyData *pData, void *pStruct, void *pOut);

private:
	static bool s_Detected;
	static std::map<int, std::function<Callback>> s_Listeners;
	static int s_MaxListener;

	static void NotifyListeners();
	static void SetDetected(bool value, bool force_broadcast = false);
};

class SrcTVPlusListener
{
public:
	template<typename F> SrcTVPlusListener(F&& cb) {
		m_ListenerID = SrcTVPlus::register_listener(std::forward<F>(cb));
	}
	SrcTVPlusListener() {
		SrcTVPlus::unregister_listener(m_ListenerID);
	}

private:
	int m_ListenerID;
};