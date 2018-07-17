#pragma once
#include "PluginBase/EntityOffset.h"

#include <memory>

#include <ehandle.h>

class C_BaseEntity;
enum class TFTeam;

class TFTeamResource final
{
	TFTeamResource() = default;

public:
	~TFTeamResource() = default;
	static std::shared_ptr<TFTeamResource> GetTeamResource();

	int GetTeamScore(TFTeam team);

private:
	static uint_fast8_t ToTeamIndex(TFTeam team);

	// All arrays are [red, blue]
	EntityOffset<int> m_TeamScores[2];
	CHandle<C_BaseEntity> m_TeamResourceEntity[2];
	static std::shared_ptr<TFTeamResource> s_TeamResource;
};