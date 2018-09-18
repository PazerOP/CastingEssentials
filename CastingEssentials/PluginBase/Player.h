#pragma once
#include "PluginBase/EntityOffset.h"
#include "PluginBase/PlayerStateBase.h"

#include <shared/ehandle.h>
#include <steam/steamclientpublic.h>
#include <shareddefs.h>
#include <cdll_int.h>

#include <array>
#include <string>
#include <map>
#include <memory>
#include <typeindex>

enum TFCond;
enum class TFClassType;
enum class TFTeam;
enum class TFMedigun;

class C_BaseEntity;
class C_BaseCombatWeapon;
class C_BaseAnimating;
class C_BasePlayer;
class IClientEntity;
class INetworkStringTable;

class Player final
{
public:
	static void Load();
	static void Unload();
	static Player* AsPlayer(IClientEntity* entity);
	static Player* GetPlayer(int entIndex, const char* functionName = nullptr);
	static Player* GetPlayerFromUserID(int userID);
	static Player* GetPlayerFromName(const char* exactName);
	static bool IsValidIndex(int entIndex);
	static Player* GetLocalPlayer();

	// Sorts players the same way the tournament HUD does. outBegin and outEnd must be
	// pointers to an array with at least MAX_PLAYERS slots. Returns the new end pointer.
	static Player** GetSortedPlayers(TFTeam team, Player** outBegin, Player** outEnd);

	static bool CheckDependencies();

	IClientEntity* GetEntity() const { return m_PlayerEntity.Get(); }
	C_BaseEntity* GetBaseEntity() const;
	C_BaseAnimating* GetBaseAnimating() const;
	C_BasePlayer* GetBasePlayer() const;

	bool CheckCondition(TFCond condition) const;
	TFClassType GetClass() const;
	int GetHealth() const;
	int GetMaxHealth() const;
	int GetMaxOverheal() const;
	const char* GetName() const;
	static const char* GetName(int entIndex);
	ObserverMode GetObserverMode() const;
	C_BaseEntity* GetObserverTarget() const;
	CSteamID GetSteamID() const;
	TFTeam GetTeam() const;
	int GetUserID() const;
	C_BaseCombatWeapon* GetWeapon(int i) const;
	C_BaseCombatWeapon* GetActiveWeapon() const;
	bool IsAlive() const;
	int entindex() const;
	const player_info_t& GetPlayerInfo() const;

	C_BaseCombatWeapon* GetMedigun(TFMedigun* medigunType = nullptr) const;

	Vector GetAbsOrigin() const;
	QAngle GetAbsAngles() const;
	Vector GetEyePosition() const;
	QAngle GetEyeAngles() const;

	static const Vector& GetEyeOffset(TFClassType cls);
	const Vector& GetEyeOffset() const;

	bool IsValid() const;

	template<typename T> T& GetState()
	{
		static_assert(!std::is_pointer_v<T>);
		static_assert(!std::is_reference_v<T>);

		PlayerStateBase* retVal;
		if (auto found = m_ExtraState.find(typeid(T)); found != m_ExtraState.end())
			retVal = found->second.get();
		else
			retVal = m_ExtraState.emplace(typeid(T), std::make_unique<T>(*this)).first->second.get();

		retVal->Update();
		return *static_cast<T*>(retVal);
	}

private:
	class Iterator : public std::iterator<std::forward_iterator_tag, Player*>
	{
		friend class Player;

	public:
		Iterator(const Iterator& old) = default;

		Iterator& operator++();
		Player* operator*() const { return GetPlayer(m_Index, __FUNCSIG__); }
		bool operator==(const Iterator& other) const { return m_Index == other.m_Index; }
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

	bool CheckCache() const;
	mutable IClientEntity* m_CachedPlayerEntity;
	static EntityOffset<TFTeam> s_TeamOffset;
	static EntityOffset<TFClassType> s_ClassOffset;
	static EntityOffset<int> s_HealthOffset;
	static EntityOffset<int> s_MaxHealthOffset;
	static EntityOffset<ObserverMode> s_ObserverModeOffset;
	static EntityOffset<CHandle<C_BaseEntity>> s_ObserverTargetOffset;
	static std::array<EntityOffset<CHandle<C_BaseCombatWeapon>>, MAX_WEAPONS> s_WeaponOffsets;
	static EntityOffset<CHandle<C_BaseCombatWeapon>> s_ActiveWeaponOffset;
	static std::array<EntityOffset<uint32_t>, 5> s_PlayerCondBitOffsets;

	static EntityTypeChecker s_MedigunType;

	mutable int m_CachedPlayerInfoLastUpdateFrame;
	mutable player_info_t m_CachedPlayerInfo;

	std::map<std::type_index, std::unique_ptr<PlayerStateBase>> m_ExtraState;

	static std::unique_ptr<Player> s_Players[MAX_PLAYERS];

	static int s_UserInfoChangedCallbackHook;
	static void UserInfoChangedCallbackOverride(void*, INetworkStringTable* stringTable, int stringNumber, const char* newString, const void* newData);

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
