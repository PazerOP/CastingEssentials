#pragma once
#include <memory>
#include <ehandle.h>
#include <vector>

#include <ehandle.h>

class C_BaseEntity;
class Player;

class TFPlayerResource final
{
	TFPlayerResource() = default;

public:
	~TFPlayerResource() = default;
	static std::shared_ptr<TFPlayerResource> GetPlayerResource();

	int GetMaxHealth(int playerEntIndex);
	bool IsAlive(int playerEntIndex);
	int* GetKillstreak(int playerEntIndex);
	int GetDamage(int playerEntIndex);

private:
	bool CheckEntIndex(int playerEntIndex, const char* functionName);

	CHandle<C_BaseEntity> m_PlayerResourceEntity;
	static std::shared_ptr<TFPlayerResource> m_PlayerResource;
};