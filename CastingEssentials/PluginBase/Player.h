#pragma once
#include <shared/ehandle.h>
#include <steam/steamclientpublic.h>
#include <shareddefs.h>
#include <cdll_int.h>

#include <string>
#include <memory>

enum TFCond;
enum class TFClassType;
enum class TFTeam;

class IClientEntity;
class C_BaseEntity;
class C_BaseCombatWeapon;
class C_BaseAnimating;

class Player final
{
public:
	static void Unload();
	static Player* AsPlayer(IClientEntity* entity);
	static Player* GetPlayer(int entIndex, const char* functionName = nullptr);
	static Player* GetPlayerFromUserID(int userID);
	static bool IsValidIndex(int entIndex);
	static Player* GetLocalPlayer();
	static Player* GetLocalObserverTarget();

	static bool CheckDependencies();
	static bool IsNameRetrievalAvailable() { return s_NameRetrievalAvailable; }
	static bool IsSteamIDRetrievalAvailable() { return s_SteamIDRetrievalAvailable; }
	static bool IsConditionsRetrievalAvailable() { return s_ConditionsRetrievalAvailable; }

	IClientEntity* GetEntity() const { return m_PlayerEntity.Get(); }
	C_BaseEntity* GetBaseEntity() const;
	C_BaseAnimating* GetBaseAnimating() const;

	bool CheckCondition(TFCond condition) const;
	TFClassType GetClass() const;
	int GetHealth() const;
	int GetMaxHealth() const;
	const char* GetName() const;
	static const char* GetName(int entIndex);
	int GetObserverMode() const;
	C_BaseEntity* GetObserverTarget() const;
	CSteamID GetSteamID() const;
	TFTeam GetTeam() const;
	int GetUserID() const;
	C_BaseCombatWeapon* GetWeapon(int i) const;
	C_BaseCombatWeapon* GetActiveWeapon() const;
	bool IsAlive() const;
	int entindex() const;
	const player_info_t& GetPlayerInfo() const;

	Vector GetAbsOrigin() const;
	QAngle GetAbsAngles() const;
	Vector GetEyePosition() const;
	QAngle GetEyeAngles() const;

	bool IsValid() const;

private:
	class Iterator
	{
		friend class Player;

	public:
		Iterator(const Iterator& old) { m_Index = old.m_Index; }

		Iterator& operator++();
		Player* operator*() const { return GetPlayer(m_Index, __FUNCSIG__); }
		//bool operator==(const Iterator& other) const;
		bool operator!=(const Iterator& other) const { return m_Index != other.m_Index; }

		Iterator();

	private:
		Iterator(int index) { m_Index = index; }
		int m_Index;
	};

	Player() = delete;
	Player(CHandle<IClientEntity> handle, int userID);
	Player(const Player& other) = delete;
	Player& operator=(const Player& other) = delete;

	const CHandle<IClientEntity> m_PlayerEntity;
	const int m_UserID;

	static bool s_ClassRetrievalAvailable;
	static bool s_ComparisonAvailable;
	static bool s_ConditionsRetrievalAvailable;
	static bool s_NameRetrievalAvailable;
	static bool s_SteamIDRetrievalAvailable;
	static bool s_UserIDRetrievalAvailable;

	bool CheckCache() const;
	mutable void* m_CachedPlayerEntity;
	mutable TFTeam* m_CachedTeam;
	mutable TFClassType* m_CachedClass;
	mutable int* m_CachedHealth;
	mutable int* m_CachedObserverMode;
	mutable CHandle<C_BaseEntity>* m_CachedObserverTarget;
	mutable CHandle<C_BaseCombatWeapon>* m_CachedWeapons[MAX_WEAPONS];
	mutable CHandle<C_BaseCombatWeapon>* m_CachedActiveWeapon;

	mutable uint32_t m_CachedPlayerInfoLastUpdateFrame;
	mutable player_info_t m_CachedPlayerInfo;

	mutable uint32_t m_LastValidatedFrame;

	static std::unique_ptr<Player> s_Players[ABSOLUTE_PLAYER_LIMIT];

public:
	static Iterator begin() { return Iterator(); }
	static Iterator end();

	class Iterable
	{
	public:
		Iterator begin() { return Player::begin(); }
		Iterator end() { return Player::end(); }
	};
};