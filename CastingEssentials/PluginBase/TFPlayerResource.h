#pragma once

#include "PluginBase/EntityOffset.h"

#include <array>
#include <memory>
#include <ehandle.h>
#include <vector>

#include <ehandle.h>
#include <shared/shareddefs.h>

class C_BaseEntity;
class Player;

class TFPlayerResource final
{
	TFPlayerResource();

public:
	~TFPlayerResource() = default;
	static std::shared_ptr<TFPlayerResource> GetPlayerResource();

	int GetMaxHealth(int playerEntIndex);
	bool IsAlive(int playerEntIndex);
	int* GetKillstreak(int playerEntIndex, uint_fast8_t weapon);
	int GetDamage(int playerEntIndex);

	static constexpr auto STREAK_WEAPONS = 4;

private:
	bool CheckEntIndex(int playerEntIndex, const char* functionName);

	CHandle<C_BaseEntity> m_PlayerResourceEntity;
	static std::shared_ptr<TFPlayerResource> m_PlayerResource;

	std::array<EntityOffset<bool>, MAX_PLAYERS> m_AliveOffsets;
	EntityOffset<int> m_StreakOffsets[MAX_PLAYERS][STREAK_WEAPONS];
	std::array<EntityOffset<int>, MAX_PLAYERS> m_DamageOffsets;
	std::array<EntityOffset<int>, MAX_PLAYERS> m_MaxHealthOffsets;
};