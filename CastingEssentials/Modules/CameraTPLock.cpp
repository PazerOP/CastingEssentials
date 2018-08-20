#include "Modules/Camera/CameraStateDelta.h"
#include "Modules/CameraState.h"
#include "Modules/CameraTPLock.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"

#include <client/c_baseentity.h>
#include <client/cliententitylist.h>
#include <shared/shareddefs.h>

static IClientEntityList* s_ClientEntityList;

MODULE_REGISTER(CameraTPLock);

CameraTPLock::CameraTPLock() :
	ce_tplock_enable("ce_tplock_enable", "0", FCVAR_NONE, "Locks view angles in spec_mode 5 (thirdperson/chase) to always looking the same direction as the spectated player."),
	ce_tplock_taunt_enable("ce_tplock_taunt_enable", "0", FCVAR_NONE, "Force the camera into thirdperson tplock when taunting. Does not require ce_tplock_enable."),

	ce_tplock_default_pos("ce_tplock_default_pos", "18 -80 20", FCVAR_NONE, "Camera x/y/z offset from ce_tplock_bone when tplock is active.",
		[](IConVar* var, const char* old, float) { ParseTPLockValuesInto(static_cast<ConVar*>(var), old, GetModule()->m_TPLockDefault.m_Pos); }),
	ce_tplock_default_angle("ce_tplock_default_angle", "*0.5 ? =0", FCVAR_NONE, "Camera angle offset (pitch/yaw/roll) from ce_tplock_bone when tplock is active. See wiki for more information.",
		[](IConVar* var, const char* old, float) { ParseTPLockValuesInto(static_cast<ConVar*>(var), old, GetModule()->m_TPLockDefault.m_Angle); }),
	ce_tplock_default_dps("ce_tplock_default_dps", "-1 -1 -1", FCVAR_NONE, "Max degrees per second for angle (pitch/yaw/roll) when tplock is active. Set < 0 to uncap.",
		[](IConVar* var, const char* old, float) { ParseTPLockValuesInto(static_cast<ConVar*>(var), old, GetModule()->m_TPLockDefault.m_DPS); }),

	ce_tplock_taunt_pos("ce_tplock_taunt_pos", "0 -80 -15", FCVAR_NONE, "Camera x/y/z offset from ce_tplock_bone when taunting with taunt tplock enabled.",
		[](IConVar* var, const char* old, float) { ParseTPLockValuesInto(static_cast<ConVar*>(var), old, GetModule()->m_TPLockTaunt.m_Pos); }),
	ce_tplock_taunt_angle("ce_tplock_taunt_angle", "=15 180 =0", FCVAR_NONE, "Camera angle offset (pitch/yaw/roll) from ce_tplock_bone when taunting with taunt tplock enabled.",
		[](IConVar* var, const char* old, float) { ParseTPLockValuesInto(static_cast<ConVar*>(var), old, GetModule()->m_TPLockTaunt.m_Angle); }),
	ce_tplock_taunt_dps("ce_tplock_taunt_dps", "-1 -1 -1", FCVAR_NONE, "Max degrees per second for angle (pitch/yaw/roll) when taunting with taunt tplock enabled. Set < 0 to uncap.",
		[](IConVar* var, const char* old, float) { ParseTPLockValuesInto(static_cast<ConVar*>(var), old, GetModule()->m_TPLockTaunt.m_DPS); }),

	ce_tplock_bone("ce_tplock_bone", "bip_spine_2", FCVAR_NONE, "Bone to attach camera position to. Enable developer 2 for associated warnings.",
		[](IConVar* var, const char*, float) { GetModule()->TPLockBoneUpdated(static_cast<ConVar*>(var)); })
{
	m_IsTaunting = false;

	// Parse the default values
	ParseTPLockValuesInto(&ce_tplock_default_pos, ce_tplock_default_pos.GetDefault(), m_TPLockDefault.m_Pos);
	ParseTPLockValuesInto(&ce_tplock_default_angle, ce_tplock_default_angle.GetDefault(), m_TPLockDefault.m_Angle);
	ParseTPLockValuesInto(&ce_tplock_default_dps, ce_tplock_default_dps.GetDefault(), m_TPLockDefault.m_DPS);

	ParseTPLockValuesInto(&ce_tplock_taunt_pos, ce_tplock_taunt_pos.GetDefault(), m_TPLockTaunt.m_Pos);
	ParseTPLockValuesInto(&ce_tplock_taunt_angle, ce_tplock_taunt_angle.GetDefault(), m_TPLockTaunt.m_Angle);
	ParseTPLockValuesInto(&ce_tplock_taunt_dps, ce_tplock_taunt_dps.GetDefault(), m_TPLockTaunt.m_DPS);
	m_TPLockTaunt.m_Bone = m_TPLockDefault.m_Bone = ce_tplock_bone.GetString();
}

bool CameraTPLock::CheckDependencies()
{
	Modules().Depend<CameraState>();

	if (!CheckDependency(Interfaces::GetClientEntityList(), s_ClientEntityList))
		return false;

	return true;
}

void CameraTPLock::UpdateIsTaunting()
{
	m_IsTaunting = false;

	if (!ce_tplock_taunt_enable.GetBool())
		return;

	if (auto mode = CameraState::GetLocalObserverMode();
		mode != ObserverMode::OBS_MODE_IN_EYE && mode != ObserverMode::OBS_MODE_CHASE)
	{
		return;
	}

	auto player = Player::AsPlayer(CameraState::GetLocalObserverTarget());
	if (!player)
		return;

	m_IsTaunting = player->CheckCondition(TFCond::TFCond_Taunting);
}

bool CameraTPLock::ParseTPLockValues(const CCommand& valuesIn, std::array<TPLockValue, 3>& valuesOut)
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

bool CameraTPLock::ParseTPLockValuesInto(ConVar* cv, const char* oldVal, std::array<TPLockValue, 3>& values)
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

bool CameraTPLock::ParseTPLockValuesInto(ConVar* cv, const char* oldVal, std::array<float, 3>& values)
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

bool CameraTPLock::ParseTPLockValuesInto(ConVar* cv, const char* oldVal, Vector& vec)
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

void CameraTPLock::TPLockBoneUpdated(ConVar* cv)
{
	// For the time being, bone is shared between all rulesets
	m_TPLockDefault.m_Bone = m_TPLockTaunt.m_Bone = cv->GetString();
}

void CameraTPLock::SetupCameraTarget(const CamStateData& state, CameraPtr& newCamera)
{
	if (state.m_Mode != OBS_MODE_CHASE)
		return;

	if (!state.m_PrimaryTarget)
		return;

	auto tplock = std::make_shared<TPLockCamera>(state.m_PrimaryTarget);
	tplock->m_Bone = m_TPLockDefault.m_Bone;
	tplock->m_PosOffset = m_TPLockDefault.m_Pos;
	tplock->m_DPS = m_TPLockDefault.m_DPS;
	tplock->m_AngOffset = m_TPLockDefault.m_Angle;
	tplock->m_FOV = 90;

	newCamera = tplock;
}