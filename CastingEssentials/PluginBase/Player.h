#pragma once
#include <shared/ehandle.h>
#include <steam/steamclientpublic.h>
#include <string>
#include <shareddefs.h>
#include <memory>

enum TFCond;
enum class TFClassType;
enum class TFTeam;

class IClientEntity;
class C_BaseEntity;
class C_BaseCombatWeapon;

class Player final
{
public:
	static Player* AsPlayer(IClientEntity* entity);
	static Player* GetPlayer(int entIndex, const char* functionName = nullptr);

	static bool CheckDependencies();
	static bool IsNameRetrievalAvailable() { return s_NameRetrievalAvailable; }
	static bool IsSteamIDRetrievalAvailable() { return s_SteamIDRetrievalAvailable; }

	IClientEntity* GetEntity() const { return m_PlayerEntity.Get(); }

	bool CheckCondition(TFCond condition) const;
	TFClassType GetClass() const;
	int GetHealth() const;
	int GetMaxHealth() const;
	std::string GetName() const;
	int GetObserverMode() const;
	C_BaseEntity* GetObserverTarget() const;
	CSteamID GetSteamID() const;
	TFTeam GetTeam() const;
	int GetUserID() const;
	C_BaseCombatWeapon* GetWeapon(int i) const;
	bool IsAlive() const;

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

	static Iterator begin() { return Iterator(); }
	static Iterator end();

	class Iterable
	{
	public:
		Iterator begin() { return Player::begin(); }
		Iterator end() { return Player::end(); }
	};

private:
	Player() = default;
	Player(const Player& other) { Assert(0); }
	Player& operator=(const Player& other) { Assert(0); }

	CHandle<IClientEntity> m_PlayerEntity;

	static bool s_ClassRetrievalAvailable;
	static bool s_ComparisonAvailable;
	static bool s_ConditionsRetrievalAvailable;
	static bool s_NameRetrievalAvailable;
	static bool s_SteamIDRetrievalAvailable;
	static bool s_UserIDRetrievalAvailable;

	bool IsValid() const;

	bool CheckCache() const;
	mutable void* m_CachedPlayerEntity;
	mutable TFTeam* m_CachedTeam;
	mutable TFClassType* m_CachedClass;
	mutable int* m_CachedObserverMode;
	mutable CHandle<C_BaseEntity>* m_CachedObserverTarget;

	static std::unique_ptr<Player> s_Players[MAX_PLAYERS];
};