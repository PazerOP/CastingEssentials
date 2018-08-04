#include "Modules/ViewAngles.h"

#include "PluginBase/Entities.h"
#include "PluginBase/Player.h"

#include <edict.h>
#include <icliententity.h>

MODULE_REGISTER(ViewAngles);

EntityOffset<float> ViewAngles::s_EyeAngles0Offset;
EntityOffset<float> ViewAngles::s_EyeAngles1Offset;
EntityOffset<float> ViewAngles::s_KartBoostOffset;
EntityOffset<float> ViewAngles::s_KartHealthOffset;

RecvProp* ViewAngles::s_EyeAngles0Prop;
RecvProp* ViewAngles::s_EyeAngles1Prop;
RecvProp* ViewAngles::s_KartBoostProp;
RecvProp* ViewAngles::s_KartHealthProp;

ViewAngles::ViewAngles() :
	ce_viewangles_enabled("ce_viewangles_enabled", "0", FCVAR_NONE, "Enabled high-precision viewangle unpacking from servers running the TF32BitAngles plugin.",
		[](IConVar* var, const char*, float) { GetModule()->ToggleEnabled(static_cast<ConVar*>(var)); })
{
}

bool ViewAngles::CheckDependencies()
{
	{
		const auto ccPlayer = Entities::GetClientClass("CTFPlayer");
		const auto playerTable = ccPlayer->m_pRecvTable;

		s_EyeAngles0Offset = Entities::GetEntityProp<float>(ccPlayer, "m_angEyeAngles[0]");
		s_EyeAngles0Prop = Entities::FindRecvProp(playerTable, "m_angEyeAngles[0]");

		s_EyeAngles1Offset = Entities::GetEntityProp<float>(ccPlayer, "m_angEyeAngles[1]");
		s_EyeAngles1Prop = Entities::FindRecvProp(playerTable, "m_angEyeAngles[1]");

		s_KartBoostOffset = Entities::GetEntityProp<float>(ccPlayer, "m_flKartNextAvailableBoost");
		s_KartBoostProp = Entities::FindRecvProp(playerTable, "m_flKartNextAvailableBoost");

		s_KartHealthOffset = Entities::GetEntityProp<float>(ccPlayer, "m_iKartHealth");
		s_KartHealthProp = Entities::FindRecvProp(playerTable, "m_iKartHealth");
	}

	return true;
}

void ViewAngles::OnTick(bool inGame)
{
	if (!inGame || !ce_viewangles_enabled.GetBool())
		return;

	for (auto* player : Player::Iterable())
	{
		auto networkable = player->GetEntity()->GetClientNetworkable();
		std::byte* base = (std::byte*)networkable->GetDataTableBasePtr();

		const auto offsetBoost = s_KartBoostOffset.GetOffset(networkable);
		const auto offsetHealth = s_KartHealthOffset.GetOffset(networkable);

		m_KartBoostMap[base + offsetBoost] = networkable;
		m_KartHealthMap[base + offsetHealth] = networkable;
	}
}

void ViewAngles::LevelShutdown()
{
	m_KartBoostMap.clear();
	m_KartHealthMap.clear();
}

void ViewAngles::ToggleEnabled(const ConVar* var)
{
	if (var->GetBool())
	{
		m_EyeAngles0Proxy.emplace(s_EyeAngles0Prop->m_ProxyFn, EyeAngles0Proxy);
		m_EyeAngles1Proxy.emplace(s_EyeAngles1Prop->m_ProxyFn, EyeAngles1Proxy);
		m_KartBoostProxy.emplace(s_KartBoostProp->m_ProxyFn, KartBoostProxy);
		m_KartHealthProxy.emplace(s_KartHealthProp->m_ProxyFn, KartHealthProxy);
	}
	else
	{
		m_EyeAngles0Proxy.reset();
		m_EyeAngles1Proxy.reset();
		m_KartBoostProxy.reset();
		m_KartHealthProxy.reset();
	}
}

void ViewAngles::EyeAngles0Proxy(const CRecvProxyData* pData, void* pStruct, void* pOut)
{
}

void ViewAngles::EyeAngles1Proxy(const CRecvProxyData* pData, void* pStruct, void* pOut)
{
}

void ViewAngles::KartBoostProxy(const CRecvProxyData* pData, void* pStruct, void* pOut)
{
	const auto& module = GetModule();
	const auto& map = module->m_KartBoostMap;
	if (auto found = map.find(pOut); found != map.end())
		s_EyeAngles0Offset.GetValue(found->second) = pData->m_Value.m_Float;
	else
		module->m_KartBoostProxy->GetOldValue()(pData, pStruct, pOut);
}

void ViewAngles::KartHealthProxy(const CRecvProxyData* pData, void* pStruct, void* pOut)
{
	const auto& module = GetModule();
	const auto& map = module->m_KartHealthMap;
	if (auto found = map.find(pOut); found != map.end())
		s_EyeAngles1Offset.GetValue(found->second) = pData->m_Value.m_Float;
	else
		module->m_KartHealthProxy->GetOldValue()(pData, pStruct, pOut);
}
