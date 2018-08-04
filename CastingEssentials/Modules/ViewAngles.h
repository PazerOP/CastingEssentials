#pragma once
#include "PluginBase/EntityOffset.h"
#include "PluginBase/Modules.h"

#include <convar.h>
#include <dt_recv.h>

#include <optional>
#include <map>

class ConCommand;
class IConVar;

class ViewAngles final : public Module<ViewAngles>
{
public:
	ViewAngles();

	static bool CheckDependencies();
	static constexpr __forceinline const char* GetModuleName() { return "High-Precision View Angles"; }

protected:
	void OnTick(bool inGame) override;
	void LevelShutdown() override;

private:
	static EntityOffset<float> s_EyeAngles0Offset;
	static EntityOffset<float> s_EyeAngles1Offset;
	static EntityOffset<float> s_KartBoostOffset;
	static EntityOffset<float> s_KartHealthOffset;

	static RecvProp* s_EyeAngles0Prop;
	static RecvProp* s_EyeAngles1Prop;
	static RecvProp* s_KartBoostProp;
	static RecvProp* s_KartHealthProp;

	ConVar ce_viewangles_enabled;
	void ToggleEnabled(const ConVar *var);

	std::optional<VariablePusher<RecvVarProxyFn>> m_EyeAngles0Proxy;
	std::optional<VariablePusher<RecvVarProxyFn>> m_EyeAngles1Proxy;
	std::optional<VariablePusher<RecvVarProxyFn>> m_KartBoostProxy;
	std::optional<VariablePusher<RecvVarProxyFn>> m_KartHealthProxy;

	std::map<void*, IClientNetworkable*> m_KartBoostMap;
	std::map<void*, IClientNetworkable*> m_KartHealthMap;

	static void EyeAngles0Proxy(const CRecvProxyData* pData, void* pStruct, void* pOut);
	static void EyeAngles1Proxy(const CRecvProxyData* pData, void* pStruct, void* pOut);
	static void KartBoostProxy(const CRecvProxyData* pData, void* pStruct, void* pOut);
	static void KartHealthProxy(const CRecvProxyData* pData, void* pStruct, void* pOut);
};