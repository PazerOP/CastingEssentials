#include "TFPlayerResource.h"

#include "Interfaces.h"
#include "Entities.h"

#include <icliententitylist.h>
#include <icliententity.h>
#include <client/c_baseentity.h>
#include <client_class.h>
#include <toolframework/ienginetool.h>

std::shared_ptr<TFPlayerResource> TFPlayerResource::m_PlayerResource;

std::shared_ptr<TFPlayerResource> TFPlayerResource::GetPlayerResource()
{
	if (m_PlayerResource && m_PlayerResource->m_PlayerResourceEntity.Get())
		return m_PlayerResource;

	const auto count = Interfaces::GetClientEntityList()->GetHighestEntityIndex();
	for (int i = 0; i < count; i++)
	{
		IClientEntity* unknownEnt = Interfaces::GetClientEntityList()->GetClientEntity(i);
		if (!unknownEnt)
			continue;

		ClientClass* clClass = unknownEnt->GetClientClass();
		if (!clClass)
			continue;

		const char* name = clClass->GetName();
		if (strcmp(name, "CTFPlayerResource"))
			continue;

		m_PlayerResource = std::shared_ptr<TFPlayerResource>(new TFPlayerResource());
		m_PlayerResource->m_PlayerResourceEntity = unknownEnt->GetBaseEntity();
		return m_PlayerResource;
	}

	return nullptr;
}

TFPlayerResource::TFPlayerResource()
{
	auto cc = Entities::GetClientClass("CTFPlayerResource");

	char buf[32];
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		m_AliveOffsets[i] = Entities::GetEntityProp<bool>(cc, Entities::PropIndex(buf, "m_bAlive", i));
		m_StreakOffsets[i] = Entities::GetEntityProp<int>(cc, Entities::PropIndex(buf, "m_iStreaks", i));
		m_DamageOffsets[i] = Entities::GetEntityProp<int>(cc, Entities::PropIndex(buf, "m_iDamage", i));
		m_MaxHealthOffsets[i] = Entities::GetEntityProp<int>(cc, Entities::PropIndex(buf, "m_iMaxHealth", i));
	}
}

bool TFPlayerResource::IsAlive(int playerEntIndex)
{
	if (!CheckEntIndex(playerEntIndex, __FUNCTION__))
		return false;

	return m_AliveOffsets[playerEntIndex].GetValue(m_PlayerResourceEntity.Get());
}

int* TFPlayerResource::GetKillstreak(int playerEntIndex)
{
	if (!CheckEntIndex(playerEntIndex, __FUNCTION__))
		return nullptr;

	return &m_StreakOffsets[playerEntIndex].GetValue(m_PlayerResourceEntity.Get());
}

int TFPlayerResource::GetDamage(int playerEntIndex)
{
	if (!CheckEntIndex(playerEntIndex, __FUNCTION__))
		return std::numeric_limits<int>::min();

	return m_DamageOffsets[playerEntIndex].GetValue(m_PlayerResourceEntity.Get());
}

int TFPlayerResource::GetMaxHealth(int playerEntIndex)
{
	if (!CheckEntIndex(playerEntIndex, __FUNCTION__))
		return std::numeric_limits<int>::min();

	return m_MaxHealthOffsets[playerEntIndex].GetValue(m_PlayerResourceEntity.Get());
}

bool TFPlayerResource::CheckEntIndex(int playerEntIndex, const char* functionName)
{
	if (playerEntIndex < 1 || playerEntIndex > Interfaces::GetEngineTool()->GetMaxClients())
	{
		PluginWarning("Out of range playerEntIndex %i in %s()\n", playerEntIndex, functionName);
		return false;
	}

	return true;
}