#include "Player.h"
#include "Interfaces.h"
#include "Entities.h"
#include "TFDefinitions.h"
#include "HookManager.h"
#include "Misc/HLTVCameraHack.h"
#include "Modules/ItemSchema.h"
#include <cdll_int.h>
#include <icliententity.h>
#include <steam/steam_api.h>
#include <client/c_baseentity.h>
#include <toolframework/ienginetool.h>
#include "TFPlayerResource.h"
#include <client/hltvcamera.h>
#include <client/c_basecombatweapon.h>

#undef min

EntityOffset<TFTeam> Player::s_TeamOffset;
EntityOffset<TFClassType> Player::s_ClassOffset;
EntityOffset<int> Player::s_HealthOffset;
EntityOffset<int> Player::s_MaxHealthOffset;
EntityOffset<ObserverMode> Player::s_ObserverModeOffset;
EntityOffset<CHandle<C_BaseEntity>> Player::s_ObserverTargetOffset;
std::array<EntityOffset<CHandle<C_BaseCombatWeapon>>, MAX_WEAPONS> Player::s_WeaponOffsets;
EntityOffset<CHandle<C_BaseCombatWeapon>> Player::s_ActiveWeaponOffset;
std::array<EntityOffset<uint32_t>, 5> Player::s_PlayerCondBitOffsets;

EntityTypeChecker Player::s_MedigunType;

std::unique_ptr<Player> Player::s_Players[MAX_PLAYERS];

int Player::s_UserInfoChangedCallbackHook;

Player::Player(CHandle<IClientEntity> handle, int userID) : m_PlayerEntity(handle), m_UserID(userID)
{
	Assert(dynamic_cast<C_BaseEntity*>(handle.Get()));
	m_CachedPlayerEntity = nullptr;
}

void Player::Load()
{
	if (!s_UserInfoChangedCallbackHook)
	{
		s_UserInfoChangedCallbackHook = GetHooks()->AddHook<HookFunc::Global_UserInfoChangedCallback>(std::bind(&Player::UserInfoChangedCallbackOverride, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
	}
}

void Player::Unload()
{
	if (s_UserInfoChangedCallbackHook && GetHooks()->RemoveHook<HookFunc::Global_UserInfoChangedCallback>(s_UserInfoChangedCallbackHook, __FUNCSIG__))
		s_UserInfoChangedCallbackHook = 0;

	for (size_t i = 0; i < std::size(s_Players); i++)
		s_Players[i].reset();
}

bool Player::CheckDependencies()
{
	bool ready = true;

	if (!Interfaces::GetClientEntityList())
	{
		PluginWarning("Required interface IClientEntityList for player helper class not available!\n");
		ready = false;
	}

	if (!Interfaces::GetEngineTool())
	{
		PluginWarning("Required interface IEngineTool for player helper class not available!\n");
		ready = false;
	}

	if (!Interfaces::GetEngineClient())
		PluginWarning("Interface IVEngineClient for player helper class not available (required for retrieving certain info)!\n");

	if (!Interfaces::GetHLTVCamera())
	{
		PluginWarning("Interface C_HLTVCamera for player helper class not available (required for retrieving spectated player)!\n");
		ready = false;
	}

	if (!Interfaces::AreSteamLibrariesAvailable())
		PluginWarning("Steam libraries for player helper class not available (required for accuracy in retrieving Steam IDs)!\n");

	//try
	{
		const auto playerClass = Entities::GetClientClass("CTFPlayer");

		s_PlayerCondBitOffsets[0] = Entities::GetEntityProp<uint32_t>(playerClass, "_condition_bits");
		s_PlayerCondBitOffsets[1] = Entities::GetEntityProp<uint32_t>(playerClass, "m_nPlayerCond");
		s_PlayerCondBitOffsets[2] = Entities::GetEntityProp<uint32_t>(playerClass, "m_nPlayerCondEx");
		s_PlayerCondBitOffsets[3] = Entities::GetEntityProp<uint32_t>(playerClass, "m_nPlayerCondEx2");
		s_PlayerCondBitOffsets[4] = Entities::GetEntityProp<uint32_t>(playerClass, "m_nPlayerCondEx3");

		s_ClassOffset = Entities::GetEntityProp<TFClassType>(playerClass, "m_iClass");
		s_TeamOffset = Entities::GetEntityProp<TFTeam>(playerClass, "m_iTeamNum");

		char buffer[32];
		for (size_t i = 0; i < s_WeaponOffsets.size(); i++)
			s_WeaponOffsets[i] = Entities::GetEntityProp<CHandle<C_BaseCombatWeapon>>(playerClass, Entities::PropIndex(buffer, "m_hMyWeapons", i));

		s_HealthOffset = Entities::GetEntityProp<int>(playerClass, "m_iHealth");

		s_ObserverModeOffset = Entities::GetEntityProp<ObserverMode>(playerClass, "m_iObserverMode");
		s_ObserverTargetOffset = Entities::GetEntityProp<EHANDLE>(playerClass, "m_hObserverTarget");

		s_ActiveWeaponOffset = Entities::GetEntityProp<CHandle<C_BaseCombatWeapon>>(playerClass, "m_hActiveWeapon");

		s_MedigunType = Entities::GetTypeChecker("CWeaponMedigun");
	}
	//catch (const invalid_class_prop& ex)
	//{
	//	PluginWarning(ex.what());
	//}

	return ready;
}

CSteamID Player::GetSteamID() const
{
	if (IsValid())
	{
		player_info_t playerInfo;

		if (Interfaces::GetEngineClient()->GetPlayerInfo(GetEntity()->entindex(), &playerInfo))
		{
			if (playerInfo.friendsID)
			{
				static EUniverse universe = k_EUniverseInvalid;

				if (universe == k_EUniverseInvalid)
				{
					if (Interfaces::GetSteamAPIContext()->SteamUtils())
						universe = Interfaces::GetSteamAPIContext()->SteamUtils()->GetConnectedUniverse();
					else
					{
						// let's just assume that it's public - what are the chances that there's a Valve employee testing this on another universe without Steam?
						PluginWarning("Steam libraries not available - assuming public universe for user Steam IDs!\n");
						universe = k_EUniversePublic;
					}
				}

				return CSteamID(playerInfo.friendsID, 1, universe, k_EAccountTypeIndividual);
			}
		}
	}

	return CSteamID();
}

C_BaseEntity * Player::GetBaseEntity() const
{
	auto entity = GetEntity();
	return entity ? entity->GetBaseEntity() : nullptr;
}

C_BaseAnimating * Player::GetBaseAnimating() const
{
	auto entity = GetBaseEntity();
	return entity ? entity->GetBaseAnimating() : nullptr;
}

C_BasePlayer* Player::GetBasePlayer() const
{
	return dynamic_cast<C_BasePlayer*>(GetEntity());
}

bool Player::CheckCondition(TFCond condition) const
{
	if (IsValid())
	{
		CheckCache();

		auto ent = GetEntity();

		if (condition < 32)
			return (s_PlayerCondBitOffsets[0].GetValue(ent) | s_PlayerCondBitOffsets[1].GetValue(ent)) & (1 << condition);
		else if (condition < 64)
			return s_PlayerCondBitOffsets[2].GetValue(ent) & (1 << (condition - 32));
		else if (condition < 96)
			return s_PlayerCondBitOffsets[3].GetValue(ent) & (1 << (condition - 64));
		else if (condition < 128)
			return s_PlayerCondBitOffsets[4].GetValue(ent) & (1 << (condition - 96));
	}

	return false;
}

TFTeam Player::GetTeam() const
{
	if (IsValid())
	{
		if (auto ent = GetEntity())
			return s_TeamOffset.GetValue(ent);
	}

	return TFTeam::Unassigned;
}

int Player::GetUserID() const
{
	return GetPlayerInfo().userID;
}

const char* Player::GetName() const
{
	if (IsValid())
		return GetPlayerInfo().name;

	return __FUNCSIG__ ": [UNKNOWN]";
}

const char* Player::GetName(int entIndex)
{
	Player* player = GetPlayer(entIndex, __FUNCSIG__);
	if (player)
		return player->GetName();

	return __FUNCSIG__ ": [UNKNOWN]";
}

TFClassType Player::GetClass() const
{
	if (IsValid())
	{
		CheckCache();
		return s_ClassOffset.GetValue(m_CachedPlayerEntity);
	}

	return TFClassType::Unknown;
}

int Player::GetHealth() const
{
	if (IsValid())
	{
		CheckCache();
		return s_HealthOffset.GetValue(m_CachedPlayerEntity);
	}

	Assert(!"Called " __FUNCTION__ "() on an invalid player!");
	return 0;
}

int Player::GetMaxHealth() const
{
	if (IsValid())
	{
		auto playerResource = TFPlayerResource::GetPlayerResource();
		if (playerResource)
			return playerResource->GetMaxHealth(entindex());

		Assert(!"Attempted to GetMaxHealth on a player, but unable to GetPlayerResource!");
	}
	else
	{
		Assert(!"Called " __FUNCTION__ "() on an invalid player!");
	}

	return 1;	// So we avoid dividing by zero somewhere
}

int Player::GetMaxOverheal() const
{
	return (int(GetMaxHealth() * 1.5f) / 5) * 5;
}

bool Player::IsValid() const
{
	if (!IsValidIndex(entindex()))
		return false;

	if (!GetEntity())
		return false;

	return true;
}

bool Player::CheckCache() const
{
	Assert(IsValid());

	auto currentPlayerEntity = m_PlayerEntity.Get();
	if (currentPlayerEntity != m_CachedPlayerEntity)
	{
		m_CachedPlayerEntity = currentPlayerEntity;

		m_CachedPlayerInfoLastUpdateFrame = 0;

		return true;
	}

	return false;
}

void Player::UserInfoChangedCallbackOverride(void*, INetworkStringTable* stringTable, int stringNumber, const char* newString, const void* newData)
{
	// If there's any changes, force a recreation of the Player instance
	Assert(stringNumber >= 0 && stringNumber < (int)std::size(s_Players));
	s_Players[stringNumber].reset();
}

Player::Iterator Player::end()
{
	return Player::Iterator(Interfaces::GetEngineTool()->GetMaxClients() + 1);
}

Player::Iterator::Iterator()
{
	// Find the first valid player
	for (int i = 1; i <= Interfaces::GetEngineTool()->GetMaxClients(); i++)
	{
		auto player = GetPlayer(i);
		if (!player || !player->IsValid())
			continue;

		m_Index = i;
		Assert((*(*this))->GetEntity());
		return;
	}

	m_Index = Interfaces::GetEngineTool()->GetMaxClients() + 1;
}

Player::Iterator& Player::Iterator::operator++()
{
	// Find the next valid player
	for (int i = m_Index + 1; i <= Interfaces::GetEngineTool()->GetMaxClients(); i++)
	{
		auto player = GetPlayer(i);
		if (!player || !player->IsValid())
			continue;

		m_Index = i;

		Assert((*(*this))->GetEntity());
		return *this;
	}

	m_Index = Interfaces::GetEngineTool()->GetMaxClients() + 1;
	return *this;
}

bool Player::IsValidIndex(int entIndex)
{
	const auto maxclients = Interfaces::GetEngineTool()->GetMaxClients();
	if (entIndex < 1 || entIndex > maxclients)
		return false;

	return true;
}

Player* Player::GetLocalPlayer()
{
	const auto localPlayerIndex = Interfaces::GetEngineClient()->GetLocalPlayer();
	if (!IsValidIndex(localPlayerIndex))
		return nullptr;

	return GetPlayer(localPlayerIndex, __FUNCSIG__);
}

Player* Player::GetPlayer(int entIndex, const char* functionName)
{
	if (!IsValidIndex(entIndex))
	{
		if (!functionName)
			functionName = "<UNKNOWN>";

		PluginWarning("Out of range playerEntIndex %i in %s\n", entIndex, functionName);
		return nullptr;
	}

	Assert((entIndex - 1) >= 0 && (entIndex - 1) < MAX_PLAYERS);

	Player* p = s_Players[entIndex - 1].get();
	if (!p || !p->IsValid())
	{
		IClientEntity* playerEntity = Interfaces::GetClientEntityList()->GetClientEntity(entIndex);
		if (!playerEntity)
			return nullptr;

		player_info_t info;
		if (!GetHooks()->GetOriginal<HookFunc::IVEngineClient_GetPlayerInfo>()(entIndex, &info))
			return nullptr;

		s_Players[entIndex - 1] = std::unique_ptr<Player>(p = new Player(playerEntity, info.userID));

		// Check again
		if (!p || !p->IsValid())
			return nullptr;
	}

	return p;
}

Player* Player::GetPlayerFromUserID(int userID)
{
	for (Player* player : Player::Iterable())
	{
		if (player->GetUserID() == userID)
			return player;
	}

	return nullptr;
}

Player* Player::GetPlayerFromName(const char* exactName)
{
	for (Player* player : Player::Iterable())
	{
		if (!strcmp(player->GetName(), exactName))
			return player;
	}

	return nullptr;
}

Player* Player::AsPlayer(IClientEntity* entity)
{
	if (!entity)
		return nullptr;

	const int entIndex = entity->entindex();
	if (entIndex >= 1 && entIndex <= Interfaces::GetEngineTool()->GetMaxClients())
		return GetPlayer(entIndex);

	return nullptr;
}

bool Player::IsAlive() const
{
	if (IsValid())
	{
		auto playerResource = TFPlayerResource::GetPlayerResource();
		if (playerResource)
			return playerResource->IsAlive(m_PlayerEntity.GetEntryIndex());

		Assert(!"Attempted to call IsAlive on a player, but unable to GetPlayerResource!");
	}
	else
	{
		Assert(!"Called " __FUNCTION__ "() on an invalid player!");
	}

	return false;
}

int Player::entindex() const
{
	if (m_PlayerEntity.IsValid())
		return m_PlayerEntity.GetEntryIndex();
	else
		return -1;
}

const player_info_t& Player::GetPlayerInfo() const
{
	static player_info_t s_InvalidPlayerInfo = []()
	{
		player_info_t retVal;
		strcpy_s(retVal.name, "INVALID");
		retVal.userID = -1;
		strcpy_s(retVal.guid, "[U:0:0]");
		retVal.friendsID = 0;
		strcpy_s(retVal.friendsName, "INVALID");
		retVal.fakeplayer = true;
		retVal.ishltv = false;

		for (auto& crc : retVal.customFiles)
			crc = 0;

		retVal.filesDownloaded = 0;

		return retVal;
	}();

	const auto framecount = Interfaces::GetEngineTool()->HostFrameCount();
	if (m_CachedPlayerInfoLastUpdateFrame == framecount)
		return m_CachedPlayerInfo;

	if (Interfaces::GetEngineClient()->GetPlayerInfo(entindex(), &m_CachedPlayerInfo))
	{
		m_CachedPlayerInfoLastUpdateFrame = framecount;
		return m_CachedPlayerInfo;
	}

	return s_InvalidPlayerInfo;
}

C_BaseCombatWeapon* Player::GetMedigun(TFMedigun* medigunType) const
{
	if (!IsValid())
		goto Failed;

	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		C_BaseCombatWeapon* weapon = GetWeapon(i);
		if (!weapon || !s_MedigunType.Match(weapon))
			continue;

		if (medigunType)
		{
			const auto itemdefIndex = Entities::GetItemDefinitionIndex(weapon);

			const auto baseID = ItemSchema::GetModule()->GetBaseItemID(itemdefIndex);

			switch (baseID)
			{
				case 29:	*medigunType = TFMedigun::MediGun; break;
				case 35:	*medigunType = TFMedigun::Kritzkrieg; break;
				case 411:	*medigunType = TFMedigun::QuickFix; break;
				case 998:	*medigunType = TFMedigun::Vaccinator; break;
				default:	*medigunType = TFMedigun::Unknown; break;
			}
		}

		return weapon;
	}

Failed:
	if (medigunType)
		*medigunType = TFMedigun::Unknown;

	return nullptr;
}

Vector Player::GetAbsOrigin() const
{
	if (!IsValid())
		return vec3_origin;

	IClientEntity* const clientEntity = GetEntity();
	if (!clientEntity)
		return vec3_origin;

	return clientEntity->GetAbsOrigin();
}

QAngle Player::GetAbsAngles() const
{
	if (!IsValid())
		return vec3_angle;

	IClientEntity* const clientEntity = GetEntity();
	if (!clientEntity)
		return vec3_angle;

	return clientEntity->GetAbsAngles();
}

Vector Player::GetEyePosition() const
{
	if (auto baseEnt = GetBaseEntity())
		return baseEnt->EyePosition();

	return vec3_origin;
}

QAngle Player::GetEyeAngles() const
{
	if (auto baseEnt = GetBaseEntity())
		return baseEnt->EyeAngles();

	return vec3_angle;
}

const Vector& Player::GetEyeOffset(TFClassType cls)
{
	static constexpr Vector VIEW_OFFSETS[] =
	{
		Vector(0, 0, 72),		// TF_CLASS_UNDEFINED

		Vector(0, 0, 65),		// TF_CLASS_SCOUT,			// TF_FIRST_NORMAL_CLASS
		Vector(0, 0, 75),		// TF_CLASS_SNIPER,
		Vector(0, 0, 68),		// TF_CLASS_SOLDIER,
		Vector(0, 0, 68),		// TF_CLASS_DEMOMAN,
		Vector(0, 0, 75),		// TF_CLASS_MEDIC,
		Vector(0, 0, 75),		// TF_CLASS_HEAVYWEAPONS,
		Vector(0, 0, 68),		// TF_CLASS_PYRO,
		Vector(0, 0, 75),		// TF_CLASS_SPY,
		Vector(0, 0, 68),		// TF_CLASS_ENGINEER,		// TF_LAST_NORMAL_CLASS
	};

	return VIEW_OFFSETS[(int)cls];
}

const Vector& Player::GetEyeOffset() const
{
	if (!IsValid())
		return vec3_origin;

	return GetEyeOffset(GetClass());
}

ObserverMode Player::GetObserverMode() const
{
	if (IsValid())
	{
		CheckCache();
		return s_ObserverModeOffset.GetValue(m_CachedPlayerEntity);
	}

	return OBS_MODE_NONE;
}

C_BaseEntity *Player::GetObserverTarget() const
{
	if (IsValid())
	{
		CheckCache();
		return s_ObserverTargetOffset.GetValue(m_CachedPlayerEntity);
	}

	return GetEntity() ? GetEntity()->GetBaseEntity() : nullptr;
}

C_BaseCombatWeapon *Player::GetWeapon(int i) const
{
	if (i < 0 || i >= MAX_WEAPONS)
	{
		PluginWarning("Out of range index %i in %s\n", i, __FUNCTION__);
		return nullptr;
	}

	if (IsValid())
	{
		CheckCache();
		return s_WeaponOffsets[i].GetValue(m_CachedPlayerEntity);
	}

	return nullptr;
}

C_BaseCombatWeapon* Player::GetActiveWeapon() const
{
	if (IsValid())
	{
		CheckCache();
		return s_ActiveWeaponOffset.GetValue(m_CachedPlayerEntity);
	}

	return nullptr;
}
