#include "Player.h"
#include "Interfaces.h"
#include "Entities.h"
#include "TFDefinitions.h"
#include "HookManager.h"
#include <cdll_int.h>
#include <icliententity.h>
#include <steam/steam_api.h>
#include <client/c_baseentity.h>
#include <toolframework/ienginetool.h>
#include "TFPlayerResource.h"
#include <client/hltvcamera.h>

bool Player::s_ClassRetrievalAvailable = false;
bool Player::s_ComparisonAvailable = false;
bool Player::s_ConditionsRetrievalAvailable = false;
bool Player::s_NameRetrievalAvailable = false;
bool Player::s_SteamIDRetrievalAvailable = false;
bool Player::s_UserIDRetrievalAvailable = false;

std::unique_ptr<Player> Player::s_Players[ABSOLUTE_PLAYER_LIMIT];

Player::Player(CHandle<IClientEntity> handle, int userID) : m_PlayerEntity(handle), m_UserID(userID)
{
}

void Player::Unload()
{
	for (size_t i = 0; i < arraysize(s_Players); i++)
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

	s_ClassRetrievalAvailable = true;
	s_ComparisonAvailable = true;
	s_ConditionsRetrievalAvailable = true;
	s_NameRetrievalAvailable = true;
	s_SteamIDRetrievalAvailable = true;
	s_UserIDRetrievalAvailable = true;

	if (!Interfaces::GetEngineClient())
	{
		PluginWarning("Interface IVEngineClient for player helper class not available (required for retrieving certain info)!\n");

		s_NameRetrievalAvailable = false;
		s_SteamIDRetrievalAvailable = false;
		s_UserIDRetrievalAvailable = false;
	}

	if (!Interfaces::GetHLTVCamera())
	{
		PluginWarning("Interface C_HLTVCamera for player helper class not available (required for retrieving spectated player)!\n");
		ready = false;
	}

	if (!Interfaces::AreSteamLibrariesAvailable())
		PluginWarning("Steam libraries for player helper class not available (required for accuracy in retrieving Steam IDs)!\n");

	if (Entities::RetrieveClassPropOffset("CTFPlayer", "m_nPlayerCond") < 0)
	{
		PluginWarning("Required property m_nPlayerCond for CTFPlayer for player helper class not available!\n");
		s_ConditionsRetrievalAvailable = false;
	}

	if (Entities::RetrieveClassPropOffset("CTFPlayer", "_condition_bits") < 0)
	{
		PluginWarning("Required property _condition_bits for CTFPlayer for player helper class not available!\n");
		s_ConditionsRetrievalAvailable = false;
	}

	if (Entities::RetrieveClassPropOffset("CTFPlayer", "m_nPlayerCondEx") < 0)
	{
		PluginWarning("Required property m_nPlayerCondEx for CTFPlayer for player helper class not available!\n");
		s_ConditionsRetrievalAvailable = false;
	}

	if (Entities::RetrieveClassPropOffset("CTFPlayer", "m_nPlayerCondEx2") < 0)
	{
		PluginWarning("Required property m_nPlayerCondEx2 for CTFPlayer for player helper class not available!\n");
		s_ConditionsRetrievalAvailable = false;
	}

	if (Entities::RetrieveClassPropOffset("CTFPlayer", "m_nPlayerCondEx3") < 0)
	{
		PluginWarning("Required property m_nPlayerCondEx3 for CTFPlayer for player helper class not available!\n");
		s_ConditionsRetrievalAvailable = false;
	}

	if (Entities::RetrieveClassPropOffset("CTFPlayer", "m_iClass") < 0)
	{
		PluginWarning("Required property m_iClass for CTFPlayer for player helper class not available!\n");
		s_ClassRetrievalAvailable = false;
		s_ComparisonAvailable = false;
	}

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

bool Player::CheckCondition(TFCond condition) const
{
	if (IsValid())
	{
		uint32_t playerCond = *Entities::GetEntityProp<uint32_t *>(GetEntity(), { "m_nPlayerCond" });
		uint32_t condBits = *Entities::GetEntityProp<uint32_t *>(GetEntity(), { "_condition_bits" });
		uint32_t playerCondEx = *Entities::GetEntityProp<uint32_t *>(GetEntity(), { "m_nPlayerCondEx" });
		uint32_t playerCondEx2 = *Entities::GetEntityProp<uint32_t *>(GetEntity(), { "m_nPlayerCondEx2" });
		uint32_t playerCondEx3 = *Entities::GetEntityProp<uint32_t *>(GetEntity(), { "m_nPlayerCondEx3" });

		uint32_t conditions[4];
		conditions[0] = playerCond | condBits;
		conditions[1] = playerCondEx;
		conditions[2] = playerCondEx2;
		conditions[3] = playerCondEx3;

		if (condition < 32)
		{
			if (conditions[0] & (1 << condition))
			{
				return true;
			}
		}
		else if (condition < 64)
		{
			if (conditions[1] & (1 << (condition - 32)))
			{
				return true;
			}
		}
		else if (condition < 96)
		{
			if (conditions[2] & (1 << (condition - 64)))
			{
				return true;
			}
		}
		else if (condition < 128)
		{
			if (conditions[3] & (1 << (condition - 96)))
			{
				return true;
			}
		}
	}

	return false;
}

TFTeam Player::GetTeam() const
{
	if (IsValid())
	{
		if (CheckCache() || !m_CachedTeam)
		{
			C_BaseEntity* entity = GetEntity()->GetBaseEntity();
			if (!entity)
				return TFTeam::Unassigned;

			m_CachedTeam = Entities::GetEntityProp<TFTeam*>(entity, { "m_iTeamNum" });
		}

		if (m_CachedTeam)
			return *m_CachedTeam;
	}

	return TFTeam::Unassigned;
}

int Player::GetUserID() const
{
	if (IsValid())
	{
		player_info_t playerInfo;
		if (Interfaces::GetEngineClient()->GetPlayerInfo(GetEntity()->entindex(), &playerInfo))
			return playerInfo.userID;
	}

	return 0;
}

std::string Player::GetName() const
{
	if (IsValid())
	{
		player_info_t playerInfo;
		if (Interfaces::GetEngineClient()->GetPlayerInfo(GetEntity()->entindex(), &playerInfo))
			return playerInfo.name;
	}

	return std::string();
}

TFClassType Player::GetClass() const
{
	if (IsValid())
	{
		if (CheckCache() || !m_CachedClass)
			m_CachedClass = Entities::GetEntityProp<TFClassType*>(GetEntity(), { "m_iClass" });

		if (m_CachedClass)
			return *m_CachedClass;
	}

	return TFClassType::Unknown;
}

bool Player::IsValid() const
{
	if (!m_PlayerEntity.IsValid())
		return false;

	if (!m_PlayerEntity.Get())
		return false;

	if (entindex() < 1 || entindex() > Interfaces::GetEngineTool()->GetMaxClients())
		return false;

	{
		player_info_t info;
		if (!GetHooks()->GetOriginal<IVEngineClient_GetPlayerInfo>()(entindex(), &info))
			return false;

		if (info.userID != m_UserID)
			return false;
	}

	// pazer: this is slow, and theoretically it should be impossible to create
	// Player objects with incorrect entity types
	//if (!Entities::CheckEntityBaseclass(playerEntity, "TFPlayer"))
	//	return false;

	return true;
}

bool Player::CheckCache() const
{
	Assert(IsValid());

	void* current = m_PlayerEntity.Get();
	if (current != m_CachedPlayerEntity)
	{
		m_CachedPlayerEntity = current;

		m_CachedTeam = nullptr;
		m_CachedClass = nullptr;
		m_CachedObserverMode = nullptr;
		m_CachedObserverTarget = nullptr;

		for (auto& wpn : m_CachedWeapons)
			wpn = nullptr;

		return true;
	}

	return false;
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
		Player* p = GetPlayer(i);
		if (!p || !p->IsValid())
			continue;

		m_Index = i;
		return;
	}

	m_Index = Interfaces::GetEngineTool()->GetMaxClients() + 1;
	return;
}

Player::Iterator& Player::Iterator::operator++()
{
	// Find the next valid player
	for (int i = m_Index + 1; i < Interfaces::GetEngineTool()->GetMaxClients(); i++)
	{
		if (!GetPlayer(i) || !GetPlayer(i)->IsValid())
			continue;

		m_Index = i;
		return *this;
	}

	m_Index = Interfaces::GetEngineTool()->GetMaxClients() + 1;
	return *this;
}

bool Player::IsValidIndex(int entIndex)
{
	if (entIndex < 1 || entIndex > Interfaces::GetEngineTool()->GetMaxClients())
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

Player* Player::GetLocalObserverTarget()
{
	const Player* const localPlayer = GetLocalPlayer();
	if (!localPlayer)
		return nullptr;

	return Player::AsPlayer(localPlayer->GetObserverTarget());
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
		if (!GetHooks()->GetOriginal<IVEngineClient_GetPlayerInfo>()(entIndex, &info))
			return nullptr;

		s_Players[entIndex - 1] = std::unique_ptr<Player>(p = new Player(playerEntity, info.userID));
	}

	if (!p || !p->IsValid())
		return nullptr;

	return p;
}

Player* Player::AsPlayer(IClientEntity* entity)
{
	if (!entity)
		return nullptr;

	const int entIndex = entity->entindex();
	if (entIndex >= 1 && entIndex <= Interfaces::GetEngineTool()->GetMaxClients())
		return GetPlayer(entity->entindex());

	return nullptr;
}

bool Player::IsAlive() const
{
	if (IsValid())
	{
		auto playerResource = TFPlayerResource::GetPlayerResource();
		return playerResource->IsAlive(m_PlayerEntity.GetEntryIndex());
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

	if (!Interfaces::GetEngineClient()->GetPlayerInfo(entindex(), &m_CachedPlayerInfo))
		return s_InvalidPlayerInfo;

	return m_CachedPlayerInfo;
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
	if (!IsValid())
		return vec3_origin;

	static const Vector VIEW_OFFSETS[] =
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

	return GetAbsOrigin() + VIEW_OFFSETS[(int)GetClass()];
}

QAngle Player::GetEyeAngles() const
{
	if (!IsValid())
		return vec3_angle;

	IClientEntity* const clientEntity = GetEntity();
	if (!clientEntity)
		return vec3_angle;

	C_BaseEntity* const baseEntity = clientEntity->GetBaseEntity();
	if (!baseEntity)
		return vec3_angle;

	return baseEntity->EyeAngles();
}

int Player::GetObserverMode() const
{
	if (IsValid())
	{
		if (CheckCache() || !m_CachedObserverMode)
			m_CachedObserverMode = Entities::GetEntityProp<int*>(GetEntity(), { "m_iObserverMode" });

		if (m_CachedObserverMode)
			return *m_CachedObserverMode;
	}

	return OBS_MODE_NONE;
}

class HLTVCameraOverride final : public C_HLTVCamera
{
public:
	static C_BaseEntity* GetPrimaryTargetReimplementation()
	{
		HLTVCameraOverride* hltvCamera = (HLTVCameraOverride*)Interfaces::GetHLTVCamera();
		if (!hltvCamera)
			return nullptr;

		if (hltvCamera->m_iCameraMan > 0)
		{
			Player *pCameraMan = Player::GetPlayer(hltvCamera->m_iCameraMan, __FUNCSIG__);
			if (pCameraMan)
				return pCameraMan->GetObserverTarget();
		}

		if (hltvCamera->m_iTraget1 <= 0)
			return nullptr;

		IClientEntity* target = Interfaces::GetClientEntityList()->GetClientEntity(hltvCamera->m_iTraget1);
		return target ? target->GetBaseEntity() : nullptr;
	}

	using C_HLTVCamera::m_iCameraMan;
	using C_HLTVCamera::m_iTraget1;
};

C_BaseEntity *Player::GetObserverTarget() const
{
	if (IsValid())
	{
		if (Interfaces::GetEngineClient()->IsHLTV())
			return HLTVCameraOverride::GetPrimaryTargetReimplementation();

		if (CheckCache() || !m_CachedObserverTarget)
			m_CachedObserverTarget = Entities::GetEntityProp<EHANDLE*>(GetEntity(), { "m_hObserverTarget" });

		if (m_CachedObserverTarget)
			return m_CachedObserverTarget->Get();
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
		if (CheckCache() || !m_CachedWeapons[i])
		{
			char buffer[32];
			Entities::PropIndex(buffer, "m_hMyWeapons", i);
			m_CachedWeapons[i] = Entities::GetEntityProp<CHandle<C_BaseCombatWeapon>*>(GetEntity(), buffer);
		}

		if (m_CachedWeapons[i])
			return m_CachedWeapons[i]->Get();
	}

	return nullptr;
}