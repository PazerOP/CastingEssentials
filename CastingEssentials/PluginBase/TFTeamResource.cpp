#include "TFTeamResource.h"
#include "PluginBase/Entities.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/TFDefinitions.h"

#include <client/cliententitylist.h>
#include <client/c_baseentity.h>
#include <client_class.h>

std::shared_ptr<TFTeamResource> TFTeamResource::s_TeamResource;

std::shared_ptr<TFTeamResource> TFTeamResource::GetTeamResource()
{
	if (s_TeamResource && s_TeamResource->m_TeamResourceEntity[0].Get() && s_TeamResource->m_TeamResourceEntity[1].Get())
		return s_TeamResource;

	s_TeamResource.reset();

	const auto count = Interfaces::GetClientEntityList()->GetHighestEntityIndex();
	for (int i = 0; i < count; i++)
	{
		IClientEntity* unknownEnt = Interfaces::GetClientEntityList()->GetClientEntity(i);
		if (!unknownEnt)
			continue;

		IClientNetworkable* networkable = unknownEnt->GetClientNetworkable();
		if (!networkable)
			continue;

		ClientClass* clClass = unknownEnt->GetClientClass();
		if (!clClass)
			continue;

		const char* name = clClass->GetName();
		if (strcmp(name, "CTFTeam"))
			continue;

		if (!s_TeamResource)
			s_TeamResource.reset(new TFTeamResource());

		TFTeam team = *Entities::GetEntityTeam(networkable);
		if (team == TFTeam::Red)
			s_TeamResource->m_TeamResourceEntity[0] = unknownEnt->GetBaseEntity();
		else if (team == TFTeam::Blue)
			s_TeamResource->m_TeamResourceEntity[1] = unknownEnt->GetBaseEntity();

		if (s_TeamResource->m_TeamResourceEntity[0] && s_TeamResource->m_TeamResourceEntity[1])
			return s_TeamResource;
	}

	return nullptr;
}

int TFTeamResource::GetTeamScore(TFTeam team)
{
	const auto teamIndex = ToTeamIndex(team);

	if (!m_TeamScores[teamIndex])
		m_TeamScores[teamIndex] = Entities::GetEntityProp<int>(m_TeamResourceEntity[teamIndex].Get(), "m_iScore");

	if (m_TeamScores[teamIndex])
		return *m_TeamScores[teamIndex];
	else
	{
		Assert(!"Failed to get team score variable!");
		return 0;
	}
}

uint_fast8_t TFTeamResource::ToTeamIndex(TFTeam team)
{
	switch (team)
	{
		default:
			Assert(!"Invalid TFTeam for TFTeamResource!");
			// Return team index 0 (team red) by default

		case TFTeam::Red:	return 0;
		case TFTeam::Blue:	return 1;
	}
}
