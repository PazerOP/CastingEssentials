#include "Modules/CameraState.h"
#include "Modules/Camera/PlayerCameraGroup.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"

#include <shared/gameeventdefs.h>

static bool ParseTPLockValues(const CCommand& valuesIn, std::array<TPLockValue, 3>& valuesOut);
static bool ParseTPLockValuesInto(ConVar* cv, const char* oldVal, std::array<TPLockValue, 3>& values);
static bool ParseTPLockValuesInto(ConVar* cv, const char* oldVal, std::array<float, 3>& values);
static bool ParseTPLockValuesInto(ConVar* cv, const char* oldVal, Vector& vec);
static void TPLockBoneUpdated(const ConVar* cvar);

static bool s_InitRulesets = false;
static TPLockRuleset s_TPLockDefault;
static TPLockRuleset s_TPLockTaunt;

static ConVar ce_tplock_enable("ce_tplock_enable", "0", FCVAR_NONE, "Locks view angles in spec_mode 5 (thirdperson/chase) to always looking the same direction as the spectated player.");
ConVar ce_tplock_taunt_enable("ce_tplock_taunt_enable", "0", FCVAR_NONE, "Force the camera into thirdperson tplock when taunting. Does not require ce_tplock_enable.");

ConVar ce_tplock_default_pos("ce_tplock_default_pos", "18 -80 20", FCVAR_NONE,
	"Camera x/y/z offset from ce_tplock_bone when tplock is active.",
	[](IConVar* var, const char* old, float) { ParseTPLockValuesInto(static_cast<ConVar*>(var), old, s_TPLockDefault.m_Pos); });
ConVar ce_tplock_default_angle("ce_tplock_default_angle", "*0.5 ? =0", FCVAR_NONE,
	"Camera angle offset (pitch/yaw/roll) from ce_tplock_bone when tplock is active. See wiki for more information.",
	[](IConVar* var, const char* old, float) { ParseTPLockValuesInto(static_cast<ConVar*>(var), old, s_TPLockDefault.m_Angle); });
ConVar ce_tplock_default_dps("ce_tplock_default_dps", "-1 -1 -1", FCVAR_NONE,
	"Max degrees per second for angle (pitch/yaw/roll) when tplock is active. Set < 0 to uncap.",
	[](IConVar* var, const char* old, float) { ParseTPLockValuesInto(static_cast<ConVar*>(var), old, s_TPLockDefault.m_DPS); });

ConVar ce_tplock_taunt_pos("ce_tplock_taunt_pos", "0 -80 -15", FCVAR_NONE,
	"Camera x/y/z offset from ce_tplock_bone when taunting with taunt tplock enabled.",
	[](IConVar* var, const char* old, float) { ParseTPLockValuesInto(static_cast<ConVar*>(var), old, s_TPLockTaunt.m_Pos); });
ConVar ce_tplock_taunt_angle("ce_tplock_taunt_angle", "=15 180 =0", FCVAR_NONE,
	"Camera angle offset (pitch/yaw/roll) from ce_tplock_bone when taunting with taunt tplock enabled.",
	[](IConVar* var, const char* old, float) { ParseTPLockValuesInto(static_cast<ConVar*>(var), old, s_TPLockTaunt.m_Angle); });
ConVar ce_tplock_taunt_dps("ce_tplock_taunt_dps", "-1 -1 -1", FCVAR_NONE,
	"Max degrees per second for angle (pitch/yaw/roll) when taunting with taunt tplock enabled. Set < 0 to uncap.",
	[](IConVar* var, const char* old, float) { ParseTPLockValuesInto(static_cast<ConVar*>(var), old, s_TPLockTaunt.m_DPS); });

ConVar ce_tplock_bone("ce_tplock_bone", "bip_spine_2", FCVAR_NONE,
	"Bone to attach camera position to. Enable developer 2 for associated warnings.",
	[](IConVar* var, const char*, float) { TPLockBoneUpdated(static_cast<ConVar*>(var)); });

PlayerCameraGroup::PlayerCameraGroup(Player& player) :
	m_FirstPersonCam(player.GetBaseEntity()),
	m_TPLockCam(player.GetBaseEntity()),
	m_TPLockTauntCam(player.GetBaseEntity()),

	m_PlayerEnt(player.GetEntity())
{
	if (auto mgr = Interfaces::GetGameEventManager())
		mgr->AddListener(this, GAME_EVENT_PLAYER_DEATH, false);

	if (!s_InitRulesets)
	{
		ParseTPLockValuesInto(&ce_tplock_default_pos, "", s_TPLockDefault.m_Pos);
		ParseTPLockValuesInto(&ce_tplock_default_angle, "", s_TPLockDefault.m_Angle);
		ParseTPLockValuesInto(&ce_tplock_default_dps, "", s_TPLockDefault.m_DPS);
		ParseTPLockValuesInto(&ce_tplock_taunt_pos, "", s_TPLockTaunt.m_Pos);
		ParseTPLockValuesInto(&ce_tplock_taunt_angle, "", s_TPLockTaunt.m_Angle);
		ParseTPLockValuesInto(&ce_tplock_taunt_dps, "", s_TPLockTaunt.m_DPS);
		TPLockBoneUpdated(&ce_tplock_bone);

		s_InitRulesets = true;
	}

	m_TPLockCam.m_Ruleset = s_TPLockDefault;
	m_TPLockTauntCam.m_Ruleset = s_TPLockTaunt;

	m_TPLockCam.m_FOV = m_TPLockTauntCam.m_FOV = 90;
}

PlayerCameraGroup::~PlayerCameraGroup()
{
	if (auto mgr = Interfaces::GetGameEventManager())
		mgr->RemoveListener(this);
}

void PlayerCameraGroup::GetBestCamera(CameraPtr& targetCamera)
{
	auto player = Player::AsPlayer(m_PlayerEnt.Get());
	Assert(player);
	if (!player)
		return;

	auto camstate = CameraState::GetModule();

	ICamera* bestCamera = nullptr;

	const bool isAlive = player->IsAlive();
	if (isAlive)
		m_DeathCam.reset();

	if (!isAlive && m_DeathCam)
		bestCamera = &m_DeathCam.value();
	else if (player->CheckCondition(TFCond::TFCond_Taunting))
		bestCamera = &m_TPLockTauntCam;
	else if (camstate->GetDesiredObserverMode() == ObserverMode::OBS_MODE_CHASE)
		bestCamera = &m_TPLockCam;
	else if (camstate->GetDesiredObserverMode() == ObserverMode::OBS_MODE_IN_EYE)
		bestCamera = &m_FirstPersonCam;

	// Aliased shared_ptr
	if (bestCamera && targetCamera.get() != bestCamera)
		targetCamera = std::shared_ptr<ICamera>(shared_from_this(), bestCamera);
}

void PlayerCameraGroup::FireGameEvent(IGameEvent* event)
{
	if (strcmp(event->GetName(), GAME_EVENT_PLAYER_DEATH))
		return;

	if (event->GetInt("victim_entindex") != m_PlayerEnt.GetEntryIndex())
		return;

	m_DeathCam.emplace();
	m_DeathCam->m_Victim = m_PlayerEnt;

	auto killerUserID = event->GetInt("attacker");
	auto killerPlayer = Player::GetPlayerFromUserID(killerUserID);
	if (killerPlayer && killerPlayer->GetEntity() != m_PlayerEnt)
		m_DeathCam->m_Killer = killerPlayer->GetEntity();
}

bool ParseTPLockValues(const CCommand& valuesIn, std::array<TPLockValue, 3>& valuesOut)
{
	if (valuesIn.ArgC() != (int)valuesOut.size())
		return false;

	for (uint_fast8_t i = 0; i < valuesOut.size(); i++)
	{
		const char* valIn = valuesIn[i];
		auto& valOut = valuesOut[i];

		if (valIn[0] == '*')
		{
			valOut.m_Mode = TPLockValue::Mode::Scale;

			char* endVal;
			valOut.m_Value = strtof(&valIn[1], &endVal);

			valIn = endVal;
			if (valIn[i] == '+' || valIn[i] == '-')
			{
				valOut.m_Mode = TPLockValue::Mode::ScaleAdd;
				valOut.m_Base = strtof(valIn, nullptr);
			}
		}
		else if (valIn[0] == '=')
		{
			valOut.m_Mode = TPLockValue::Mode::Set;
			valOut.m_Value = strtof(&valIn[1], nullptr);
		}
		else if (valIn[0] == '?')
		{
			valOut.m_Mode = TPLockValue::Mode::Add;
			valOut.m_Value = 0;
		}
		else
		{
			valOut.m_Mode = TPLockValue::Mode::Add;
			valOut.m_Value = strtof(valIn, nullptr);
		}
	}

	return true;
}

bool ParseTPLockValuesInto(ConVar* cv, const char* oldVal, std::array<TPLockValue, 3>& values)
{
	CCommand cmd;
	cmd.Tokenize(cv->GetString());

	if (!ParseTPLockValues(cmd, values))
	{
		Warning("%s: Failed to parse tplock values\n", cv->GetName());
		cv->SetValue(oldVal);

		return false;
	}

	return true;
}

bool ParseTPLockValuesInto(ConVar* cv, const char* oldVal, std::array<float, 3>& values)
{
	CCommand cmd;
	cmd.Tokenize(cv->GetString());

	if (cmd.ArgC() != (int)values.size())
		goto ParseFailed;

	for (uint_fast8_t i = 0; i < values.size(); i++)
	{
		char* endPtr;
		values[i] = strtof(cmd[i], &endPtr);

		if (endPtr == cmd[i])
			goto ParseFailed;
	}

	return true;

ParseFailed:
	Warning("%s: Failed to parse 3 float values\n", cv->GetName());
	cv->SetValue(oldVal);
	return false;
}

bool ParseTPLockValuesInto(ConVar* cv, const char* oldVal, Vector& vec)
{
	std::array<float, 3> values;
	if (ParseTPLockValuesInto(cv, oldVal, values))
	{
		for (uint_fast8_t i = 0; i < 3; i++)
			vec[i] = values[i];

		return true;
	}

	return false;
}

void TPLockBoneUpdated(const ConVar* cv)
{
	// For the time being, bone is shared between all rulesets
	s_TPLockDefault.m_Bone = s_TPLockTaunt.m_Bone = cv->GetString();
}
