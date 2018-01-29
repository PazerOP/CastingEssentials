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

bool TFPlayerResource::IsAlive(int playerEntIndex)
{
	if (!CheckEntIndex(playerEntIndex, __FUNCTION__))
		return false;

	char buffer[32];
	Entities::PropIndex(buffer, "m_bAlive", playerEntIndex);

	return *Entities::GetEntityProp<bool*>(dynamic_cast<C_BaseEntity *>(m_PlayerResourceEntity.Get()), buffer);
}

int* TFPlayerResource::GetKillstreak(int playerEntIndex)
{
	if (!CheckEntIndex(playerEntIndex, __FUNCTION__))
		return false;

	char buffer[32];
	Entities::PropIndex(buffer, "m_iStreaks", playerEntIndex);

	return Entities::GetEntityProp<int*>(dynamic_cast<C_BaseEntity *>(m_PlayerResourceEntity.Get()), buffer);
}

int TFPlayerResource::GetDamage(int playerEntIndex)
{
	if (!CheckEntIndex(playerEntIndex, __FUNCTION__))
		return false;

	char buffer[32];
	Entities::PropIndex(buffer, "m_iDamage", playerEntIndex);

	return *Entities::GetEntityProp<int*>(m_PlayerResourceEntity.Get(), buffer);
}

int TFPlayerResource::GetMaxHealth(int playerEntIndex)
{
	if (!CheckEntIndex(playerEntIndex, __FUNCTION__))
		return false;

	char buffer[32];
	Entities::PropIndex(buffer, "m_iMaxHealth", playerEntIndex);

	return *Entities::GetEntityProp<int*>(dynamic_cast<C_BaseEntity*>(m_PlayerResourceEntity.Get()), buffer);
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