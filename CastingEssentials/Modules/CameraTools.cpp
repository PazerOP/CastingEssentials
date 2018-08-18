#include "CameraTools.h"
#include "Misc/HLTVCameraHack.h"
#include "Modules/CameraSmooths.h"
#include "Modules/CameraState.h"
#include "Modules/FOVOverride.h"
#include "PluginBase/Entities.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"

#include <client/c_baseplayer.h>
#include <filesystem.h>
#include <shared/gamerules.h>
#include <tier1/KeyValues.h>
#include <tier3/tier3.h>
#include <cdll_int.h>
#include <client/c_baseentity.h>
#include <characterset.h>
#include <functional>
#include <client/hltvcamera.h>
#include <toolframework/ienginetool.h>
#include <util_shared.h>
#include <client/c_baseanimating.h>
#include <vprof.h>

#include <algorithm>

MODULE_REGISTER(CameraTools);

EntityOffset<float> CameraTools::s_ViewOffsetZOffset;

CameraTools::CameraTools() :
	ce_cameratools_show_mode("ce_cameratools_show_mode", "0", FCVAR_NONE, "Displays the current spec_mode in the top right corner of the screen."),

	ce_cameratools_autodirector_mode("ce_cameratools_autodirector_mode", "0", FCVAR_NONE, "Forces the camera mode to this value.",
		[](IConVar* var, const char* pOldValue, float flOldValue) { GetModule()->ChangeForceMode(var, pOldValue, flOldValue); }),
	ce_cameratools_force_target("ce_cameratools_force_target", "-1", FCVAR_NONE, "Forces the camera target to this player index.",
		[](IConVar* var, const char* pOldValue, float flOldValue) { GetModule()->ChangeForceTarget(var, pOldValue, flOldValue); }),
	ce_cameratools_force_valid_target("ce_cameratools_force_valid_target", "0", FCVAR_NONE, "Forces the camera to only have valid targets.",
		[](IConVar* var, const char* pOldValue, float flOldValue) { GetModule()->ToggleForceValidTarget(var, pOldValue, flOldValue); }),
	ce_cameratools_spec_player_alive("ce_cameratools_spec_player_alive", "1", FCVAR_NONE, "Prevents spectating dead players."),
	ce_cameratools_fix_view_heights("ce_cameratools_fix_view_heights", "0", FCVAR_NONE, "Corrects HLTV view height to match that of spectated classes."),
	ce_cameratools_disable_view_punches("ce_cameratools_disable_view_punches", "0", FCVAR_NONE,
		"Disables all view punches (used for recoil effects on some weapons)",
		[](IConVar* var, const char*, float) { GetModule()->ToggleDisableViewPunches(static_cast<ConVar*>(var)); }),

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
		[](IConVar* var, const char*, float) { GetModule()->TPLockBoneUpdated(static_cast<ConVar*>(var)); }),

	ce_cameratools_spec_entindex("ce_cameratools_spec_entindex", [](const CCommand& args) { GetModule()->SpecEntIndex(args); },
		"Spectates a player by entindex"),
	ce_cameratools_spec_pos("ce_cameratools_spec_pos", [](const CCommand& args) { GetModule()->SpecPosition(args); },
		"Moves the camera to a given position and angle."),
	ce_cameratools_spec_pos_delta("ce_cameratools_spec_pos_delta", [](const CCommand& args) { GetModule()->SpecPositionDelta(args); },
		"Offsets the camera by the given values."),
	ce_cameratools_spec_class("ce_cameratools_spec_class", [](const CCommand& args) { GetModule()->SpecClass(args); },
		"Spectates a specific class: ce_cameratools_spec_class <team> <class> [index]"),
	ce_cameratools_spec_steamid("ce_cameratools_spec_steamid", [](const CCommand& args) { GetModule()->SpecSteamID(args); },
		"Spectates a player with the given steamid: ce_cameratools_spec_steamid <steamID>"),
	ce_cameratools_spec_index("ce_cameratools_spec_index", [](const CCommand& args) { GetModule()->SpecIndex(args); },
		"Spectate a player based on their index in the tournament spectator hud."),

	ce_cameratools_show_users("ce_cameratools_show_users", [](const CCommand& args) { GetModule()->ShowUsers(args); },
		"Lists all currently connected players on the server."),

	m_SetModeHook(std::bind(&CameraTools::SetModeOverride, this, std::placeholders::_1)),
	m_SetPrimaryTargetHook(std::bind(&CameraTools::SetPrimaryTargetOverride, this, std::placeholders::_1))
{
	m_SwitchReason = ModeSwitchReason::Unknown;
	m_SpecGUISettings = new KeyValues("Resource/UI/SpectatorTournament.res");
	m_SpecGUISettings->LoadFromFile(g_pFullFileSystem, "resource/ui/spectatortournament.res", "mod");

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

bool CameraTools::CheckDependencies()
{
	Modules().Depend<CameraState>();

	bool ready = true;

	if (!Interfaces::GetEngineTool())
	{
		PluginWarning("Required interface IEngineTool for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!g_pFullFileSystem)
	{
		PluginWarning("Required interface IFileSystem for module %s not available!\n", GetModuleName());
		ready = false;
	}

	try
	{
		GetHooks()->GetRawFunc<HookFunc::C_HLTVCamera_SetCameraAngle>();
	}
	catch (bad_pointer)
	{
		PluginWarning("Required function C_HLTVCamera::SetCameraAngle for module %s not available!\n", GetModuleName());
		ready = false;
	}

	try
	{
		GetHooks()->GetRawFunc<HookFunc::C_HLTVCamera_SetMode>();
	}
	catch (bad_pointer)
	{
		PluginWarning("Required function C_HLTVCamera::SetMode for module %s not available!\n", GetModuleName());
		ready = false;
	}

	try
	{
		GetHooks()->GetRawFunc<HookFunc::C_HLTVCamera_SetPrimaryTarget>();
	}
	catch (bad_pointer)
	{
		PluginWarning("Required function C_HLTVCamera::SetPrimaryTarget for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!g_pVGuiPanel)
	{
		PluginWarning("Required interface vgui::IPanel for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!g_pVGuiSchemeManager)
	{
		PluginWarning("Required interface vgui::ISchemeManager for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Player::CheckDependencies())
	{
		PluginWarning("Required player helper class for module %s not available!\n", GetModuleName());
		ready = false;
	}

	try
	{
		Interfaces::GetClientMode();
	}
	catch (bad_pointer)
	{
		PluginWarning("Module %s requires IClientMode, which cannot be verified at this time!\n", GetModuleName());
	}

	try
	{
		Interfaces::GetHLTVCamera();
	}
	catch (bad_pointer)
	{
		PluginWarning("Module %s requires C_HLTVCamera, which cannot be verified at this time!\n", GetModuleName());
	}

	s_ViewOffsetZOffset = Entities::GetEntityProp<float>("CTFPlayer", "m_vecViewOffset[2]");

	return ready;
}

void CameraTools::SpecPosition(const Vector& pos, const QAngle& angle, ObserverMode mode, float fov)
{
	if (Interfaces::GetEngineClient()->IsHLTV())
	{
		try
		{
			HLTVCameraOverride* const hltvcamera = Interfaces::GetHLTVCamera();

			hltvcamera->SetMode(mode);
			m_SwitchReason = ModeSwitchReason::SpecPosition;

			hltvcamera->m_iCameraMan = 0;
			hltvcamera->m_vCamOrigin = pos;
			hltvcamera->m_aCamAngle = angle;
			hltvcamera->m_iTraget1 = 0;
			hltvcamera->m_iTraget2 = 0;
			hltvcamera->m_flLastAngleUpdateTime = -1;

			if (fov > 0)
				hltvcamera->m_flFOV = fov;

			// We may have to set angles directly
			if (mode == OBS_MODE_ROAMING)
			{
				QAngle wtf(angle);	// Why does this take a non-const reference
				Interfaces::GetEngineClient()->SetViewAngles(wtf);
			}

			hltvcamera->SetCameraAngle(hltvcamera->m_aCamAngle);
		}
		catch (bad_pointer &e)
		{
			Warning("%s\n", e.what());
		}
	}
	else
	{
		std::string buffer = strprintf("spec_mode %i\n", OBS_MODE_ROAMING);	// Fixed cameras do not work outside of hltv
		Interfaces::GetEngineClient()->ServerCmd(buffer.c_str());

		buffer = strprintf("spec_goto %f %f %f %f %f\n", pos.x, pos.y, pos.z, angle.x, angle.y);
		Interfaces::GetEngineClient()->ServerCmd(buffer.c_str());
	}
}

float CameraTools::CollisionTest3D(const Vector& startPos, const Vector& targetPos, float scale, const IHandleEntity* ignoreEnt)
{
	static constexpr Vector TEST_POINTS[27] =
	{
		Vector(0, 0, 0),
		Vector(0, 0, 0.5),
		Vector(0, 0, 1),

		Vector(0, 0.5, 0),
		Vector(0, 0.5, 0.5),
		Vector(0, 0.5, 1),

		Vector(0, 1, 0),
		Vector(0, 1, 0.5),
		Vector(0, 1, 1),

		Vector(0.5, 0, 0),
		Vector(0.5, 0, 0.5),
		Vector(0.5, 0, 1),

		Vector(0.5, 0.5, 0),
		Vector(0.5, 0.5, 0.5),
		Vector(0.5, 0.5, 1),

		Vector(0.5, 1, 0),
		Vector(0.5, 1, 0.5),
		Vector(0.5, 1, 1),

		Vector(1, 0, 0),
		Vector(1, 0, 0.5),
		Vector(1, 0, 1),

		Vector(1, 0.5, 0),
		Vector(1, 0.5, 0.5),
		Vector(1, 0.5, 1),

		Vector(1, 1, 0),
		Vector(1, 1, 0.5),
		Vector(1, 1, 1),
	};

	const Vector scaleVec(scale);
	const Vector mins(targetPos - Vector(scale));
	const Vector maxs(targetPos + Vector(scale));

	const Vector delta = maxs - mins;

	size_t pointsPassed = 0;
	for (const auto& testPoint : TEST_POINTS)
	{
		const Vector worldTestPoint = mins + delta * testPoint;

		trace_t tr;
		UTIL_TraceLine(startPos, worldTestPoint, MASK_VISIBLE, ignoreEnt, COLLISION_GROUP_NONE, &tr);

		if (tr.fraction >= 1)
			pointsPassed++;
	}

	return pointsPassed / float(std::size(TEST_POINTS));
}

void CameraTools::ShowUsers(const CCommand& command)
{
	std::vector<Player*> red;
	std::vector<Player*> blu;
	for (Player* player : Player::Iterable())
	{
		if (player->GetClass() == TFClassType::Unknown)
			continue;

		switch (player->GetTeam())
		{
			case TFTeam::Red:
				red.push_back(player);
				break;

			case TFTeam::Blue:
				blu.push_back(player);
				break;

			default:
				continue;
		}
	}

	Msg("%i Players:\n", red.size() + blu.size());

	for (size_t i = 0; i < red.size(); i++)
		ConColorMsg(Color(255, 128, 128, 255), "    alias player_red%i \"%s %s\"		// %s (%s)\n", i, ce_cameratools_spec_steamid.GetName(), RenderSteamID(red[i]->GetSteamID().ConvertToUint64()).c_str(), red[i]->GetName(), TF_CLASS_NAMES[(int)red[i]->GetClass()]);

	for (size_t i = 0; i < blu.size(); i++)
		ConColorMsg(Color(128, 128, 255, 255), "    alias player_blu%i \"%s %s\"		// %s (%s)\n", i, ce_cameratools_spec_steamid.GetName(), RenderSteamID(blu[i]->GetSteamID().ConvertToUint64()).c_str(), blu[i]->GetName(), TF_CLASS_NAMES[(int)blu[i]->GetClass()]);
}

bool CameraTools::ParseTPLockValues(const CCommand& valuesIn, std::array<TPLockValue, 3>& valuesOut)
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

void CameraTools::ParseTPLockValuesInto(ConVar* cv, const char* oldVal, std::array<TPLockValue, 3>& values)
{
	CCommand cmd;
	cmd.Tokenize(cv->GetString());

	if (!ParseTPLockValues(cmd, values))
	{
		Warning("%s: Failed to parse tplock values\n", cv->GetName());
		cv->SetValue(oldVal);
	}
}

void CameraTools::ParseTPLockValuesInto(ConVar* cv, const char* oldVal, std::array<float, 3>& values)
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

	return;

ParseFailed:
	Warning("%s: Failed to parse 3 float values\n", cv->GetName());
	cv->SetValue(oldVal);
}

void CameraTools::TPLockBoneUpdated(ConVar* cv)
{
	// For the time being, bone is shared between all rulesets
	m_TPLockDefault.m_Bone = m_TPLockTaunt.m_Bone = cv->GetString();
}

void CameraTools::SpecClass(const CCommand& command)
{
	// Usage: <team> <class> [classIndex]
	if (command.ArgC() < 3 || command.ArgC() > 4)
	{
		PluginWarning("%s: Expected either 2 or 3 arguments\n", command.Arg(0));
		goto Usage;
	}

	TFTeam team;
	if (!strnicmp(command.Arg(1), "blu", 3))
		team = TFTeam::Blue;
	else if (!strnicmp(command.Arg(1), "red", 3))
		team = TFTeam::Red;
	else
	{
		PluginWarning("%s: Unknown team \"%s\"\n", command.Arg(0), command.Arg(1));
		goto Usage;
	}

	TFClassType playerClass;
	if (!stricmp(command.Arg(2), "scout"))
		playerClass = TFClassType::Scout;
	else if (!stricmp(command.Arg(2), "soldier") || !stricmp(command.Arg(2), "solly"))
		playerClass = TFClassType::Soldier;
	else if (!stricmp(command.Arg(2), "pyro"))
		playerClass = TFClassType::Pyro;
	else if (!strnicmp(command.Arg(2), "demo", 4))
		playerClass = TFClassType::DemoMan;
	else if (!strnicmp(command.Arg(2), "heavy", 5) || !stricmp(command.Arg(2), "hoovy") || !stricmp(command.Arg(2), "pootis"))
		playerClass = TFClassType::Heavy;
	else if (!stricmp(command.Arg(2), "engineer") || !stricmp(command.Arg(2), "engie"))
		playerClass = TFClassType::Engineer;
	else if (!stricmp(command.Arg(2), "medic"))
		playerClass = TFClassType::Medic;
	else if (!stricmp(command.Arg(2), "sniper"))
		playerClass = TFClassType::Sniper;
	else if (!stricmp(command.Arg(2), "spy") | !stricmp(command.Arg(2), "sphee"))
		playerClass = TFClassType::Spy;
	else
	{
		PluginWarning("%s: Unknown class \"%s\"\n", command.Arg(0), command.Arg(2));
		goto Usage;
	}

	int classIndex = -1;
	if (command.ArgC() > 3 && !TryParseInteger(command.Arg(3), classIndex))
	{
		PluginWarning("%s: class index \"%s\" is not an integer\n", command.Arg(0), command.Arg(3));
		goto Usage;
	}

	SpecClass(team, playerClass, classIndex);
	return;

Usage:
	PluginWarning("Usage: %s\n", ce_cameratools_spec_class.GetHelpText());
}

void CameraTools::SpecClass(TFTeam team, TFClassType playerClass, int classIndex)
{
	int validPlayersCount = 0;
	Player* validPlayers[MAX_PLAYERS];

	for (Player* player : Player::Iterable())
	{
		if (player->GetTeam() != team || player->GetClass() != playerClass)
			continue;

		validPlayers[validPlayersCount++] = player;
	}

	if (validPlayersCount == 0)
		return;	// Nobody to switch to

	// If classIndex was not specified, cycle through the available options
	if (classIndex < 0)
	{
		auto localMode = CameraState::GetLocalObserverMode();
		if (localMode == OBS_MODE_FIXED ||
			localMode == OBS_MODE_IN_EYE ||
			localMode == OBS_MODE_CHASE)
		{
			Player* spectatingPlayer = Player::AsPlayer(CameraState::GetLocalObserverTarget());
			int currentIndex = -1;
			for (int i = 0; i < validPlayersCount; i++)
			{
				if (validPlayers[i] == spectatingPlayer)
				{
					currentIndex = i;
					break;
				}
			}

			classIndex = currentIndex + 1;
		}
	}

	if (classIndex < 0 || classIndex >= validPlayersCount)
		classIndex = 0;

	SpecPlayer(validPlayers[classIndex]->GetEntity()->entindex());
}

void CameraTools::SpecEntIndex(const CCommand& command)
{
	if (command.ArgC() != 2)
	{
		PluginWarning("%s: Expected 1 argument\n", command.Arg(0));
		goto Usage;
	}

	int index;
	if (!TryParseInteger(command.Arg(1), index))
	{
		PluginWarning("%s: entindex \"%s\" is not an integer\n", command.Arg(0), command.Arg(1));
		goto Usage;
	}

	SpecPlayer(index);
	return;

Usage:
	PluginWarning("Usage: %s <entindex>\n", command.Arg(0));
}

void CameraTools::SpecPlayer(int playerIndex)
{
	Player* player = Player::GetPlayer(playerIndex, __FUNCSIG__);
	if (!player)
	{
		Warning("%s: Unable to find a player with an entindex of %i\n", __FUNCSIG__, playerIndex);
		return;
	}

	if (!ce_cameratools_spec_player_alive.GetBool() || player->IsAlive())
	{
		if (Interfaces::GetEngineClient()->IsHLTV())
		{
			try
			{

				HLTVCameraOverride* const hltvcamera = Interfaces::GetHLTVCamera();
				if (hltvcamera)
				{
					hltvcamera->SetPrimaryTarget(player->GetEntity()->entindex());
					hltvcamera->SetMode(ce_tplock_enable.GetBool() ? OBS_MODE_CHASE : OBS_MODE_IN_EYE);
				}
			}
			catch (bad_pointer &e)
			{
				Warning("%s\n", e.what());
			}
		}
		else
		{
			char buffer[32];
			sprintf_s(buffer, "spec_player %i\n", player->GetUserID());
			Interfaces::GetEngineClient()->ServerCmd(buffer);
		}
	}
}

void CameraTools::OnTick(bool inGame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);

	AttachHooks(inGame);

	if (inGame)
	{
		if (ce_cameratools_show_mode.GetBool())
		{
			int mode = -1;
			int target = -1;
			std::string playerName;
			if (Interfaces::GetEngineClient()->IsHLTV())
			{
				mode = Interfaces::GetHLTVCamera()->GetMode();
				target = Interfaces::GetHLTVCamera()->m_iTraget1;
				if (Player::IsValidIndex(target) && Player::GetPlayer(target, __FUNCSIG__))
					playerName = Player::GetPlayer(target, __FUNCSIG__)->GetName();
			}
			else
			{
				Player* local = Player::GetLocalPlayer();
				if (local)
				{
					mode = local->GetObserverMode();
				}
			}

			if (mode >= 0)
			{
				Interfaces::GetEngineClient()->Con_NPrintf(GetConLine(), "Current spec_mode: %i %s",
					mode, mode >= 0 && mode < NUM_OBSERVER_MODES ? s_ObserverModes[mode] : "INVALID");
				Interfaces::GetEngineClient()->Con_NPrintf(GetConLine(), "Current target: %i%s",
					target, target == 0 ? " (Unspecified)" : (strprintf(" (%s)", playerName.c_str())).c_str());
			}
		}

		if (!ce_cameratools_fix_view_heights.GetBool() || !FixViewHeights())
		{
			m_OldViewHeight.reset();
			m_OldDuckViewHeight.reset();
		}

		UpdateIsTaunting();
	}
}

bool CameraTools::FixViewHeights()
{
	if (CameraState::GetLocalObserverMode() != ObserverMode::OBS_MODE_IN_EYE)
		return false;

	auto cameraPlayer = Player::AsPlayer(CameraState::GetLocalObserverTarget());
	if (!cameraPlayer)
		return false;

	auto gamerules = Interfaces::GetGameRules();
	if (!gamerules)
		return false;

	auto viewVecs = const_cast<CViewVectors*>(gamerules->GetViewVectors());
	if (!viewVecs)
		return false;

	auto ent = cameraPlayer->GetEntity();
	if (!ent)
		return false;

	auto basePlayer = dynamic_cast<C_BasePlayer*>(ent->GetBaseEntity()->GetBaseAnimating());
	if (!basePlayer)
		return false;

	// We can sometimes end up reading m_vecViewOffset[2] before the entity
	// has had a chance to update its interpolated vars. Force the interpolation
	// now.
	auto& viewOffsetVar = basePlayer->m_iv_vecViewOffset;
	viewOffsetVar.Interpolate(Interfaces::GetEngineTool()->ClientTime());

	const Vector viewOffset(0, 0, s_ViewOffsetZOffset.GetValue(ent));
	if (viewVecs->m_vView != viewOffset)
	{
		m_OldViewHeight.emplace(viewVecs->m_vView, viewOffset);
		m_OldDuckViewHeight.emplace(viewVecs->m_vDuckView, viewOffset);
	}

	return true;
}

void CameraTools::ToggleDisableViewPunches(const ConVar* var)
{
	if (var->GetBool())
	{
		auto angle = Entities::FindRecvProp("CTFPlayer", "m_vecPunchAngle");
		auto velocity = Entities::FindRecvProp("CTFPlayer", "m_vecPunchAngleVel");

		if (!angle)
		{
			PluginWarning("%s: Unable to locate RecvProp for C_TFPlayer::m_vecPunchAngle\n", var->GetName());
			return;
		}
		if (!velocity)
		{
			PluginWarning("%s: Unable to locate RecvProp for C_TFPlayer::m_vecPunchAngleVel\n", var->GetName());
			return;
		}

		RecvVarProxyFn zeroProxy = [](const CRecvProxyData* pData, void* pStruct, void* pOut)
		{
			auto outVec = reinterpret_cast<Vector*>(pOut);
			outVec->Init(0, 0, 0);
		};

		m_vecPunchAngleProxy = CreateVariablePusher(angle->m_ProxyFn, zeroProxy);
		m_vecPunchAngleVelProxy = CreateVariablePusher(velocity->m_ProxyFn, zeroProxy);
	}
	else
	{
		m_vecPunchAngleProxy.Clear();
		m_vecPunchAngleVelProxy.Clear();
	}
}

void CameraTools::UpdateIsTaunting()
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

void CameraTools::AttachHooks(bool attach)
{
	m_SetModeHook.SetEnabled(attach);
}

void CameraTools::SpecSteamID(const CCommand& command)
{
	CCommand newCommand;
	if (!ReparseForSteamIDs(command, newCommand))
		return;

	CSteamID parsed;
	if (newCommand.ArgC() != 2)
	{
		PluginWarning("%s: Expected 1 argument\n", command.Arg(0));
		goto Usage;
	}

	parsed.SetFromString(newCommand.Arg(1), k_EUniverseInvalid);
	if (!parsed.IsValid())
	{
		PluginWarning("%s: Unable to parse steamid\n", command.Arg(0));
		goto Usage;
	}

	for (Player* player : Player::Iterable())
	{
		if (player->GetSteamID() == parsed)
		{
			SpecPlayer(player->GetEntity()->entindex());
			return;
		}
	}

	Warning("%s: couldn't find a user with the steam id %s on the server\n", command.Arg(0), RenderSteamID(parsed).c_str());
	return;

Usage:
	PluginWarning("Usage: %s\n", ce_cameratools_spec_steamid.GetHelpText());
}

void CameraTools::SpecIndex(const CCommand& command)
{
	if (command.ArgC() != 3)
		goto Usage;

	TFTeam team;
	if (tolower(command.Arg(1)[0]) == 'r')
		team = TFTeam::Red;
	else if (tolower(command.Arg(1)[0]) == 'b')
		team = TFTeam::Blue;
	else
		goto Usage;

	// Create an array of all our players on the specified team
	Player* allPlayers[ABSOLUTE_PLAYER_LIMIT];
	const auto endIter = std::copy_if(Player::Iterable().begin(), Player::Iterable().end(), allPlayers,
		[team](const Player* player) { return player->GetTeam() == team; });

	const auto playerCount = std::distance(std::begin(allPlayers), endIter);

	char* endPtr;
	const auto index = std::strtol(command.Arg(2), &endPtr, 0);
	if (endPtr == command.Arg(2))
		goto Usage;	// Couldn't parse index

	if (index < 0 || index >= playerCount)
	{
		if (playerCount < 1)
			PluginWarning("%s: No players on team %s\n", command.Arg(0), command.Arg(1));
		else
			PluginWarning("Specified index \"%s\" for %s, but valid indices for team %s were [0, %i]\n",
				command.Arg(2), command.Arg(0), command.Arg(1), playerCount - 1);

		return;
	}

	// Sort by class, then by userid
	std::sort(std::begin(allPlayers), endIter, [](const Player* p1, const Player* p2)
	{
		// Convert internal classes to actual game class order
		static const auto GameClassNum = [](TFClassType codeClass)
		{
			switch (codeClass)
			{
				case TFClassType::Scout:    return 1;
				case TFClassType::Soldier:  return 2;
				case TFClassType::Pyro:     return 3;
				case TFClassType::DemoMan:  return 4;
				case TFClassType::Heavy:    return 5;
				case TFClassType::Engineer: return 6;
				case TFClassType::Medic:    return 7;
				case TFClassType::Sniper:   return 8;
				case TFClassType::Spy:      return 9;

				default:                    return 0;
			}
		};

		const auto class1 = GameClassNum(p1->GetClass());
		const auto class2 = GameClassNum(p2->GetClass());

		if (class1 < class2)
			return true;
		else if (class1 > class2)
			return false;
		else
			return p1->entindex() < p2->entindex();	// Classes identical, sort by entindex
	});

	SpecPlayer(allPlayers[index]->entindex());
	return;

Usage:
	PluginWarning("Usage: %s <red/blue> <index>\n", command.Arg(0));
}

void CameraTools::ChangeForceMode(IConVar *var, const char *pOldValue, float flOldValue)
{
	const int forceMode = ce_cameratools_autodirector_mode.GetInt();

	if (forceMode == OBS_MODE_FIXED || forceMode == OBS_MODE_IN_EYE || forceMode == OBS_MODE_CHASE || forceMode == OBS_MODE_ROAMING)
	{
		try
		{
			GetHooks()->GetOriginal<HookFunc::C_HLTVCamera_SetMode>()(forceMode);
		}
		catch (bad_pointer)
		{
			Warning("Error in setting mode.\n");
		}
	}
	else
	{
		PluginWarning("%s: Unsupported spec_mode \"%s\"", var->GetName(), ce_cameratools_autodirector_mode.GetString());
		var->SetValue(OBS_MODE_NONE);
	}
}

void CameraTools::SetModeOverride(int iMode)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	static ConVarRef spec_autodirector("spec_autodirector");
	if (spec_autodirector.GetBool())
	{
		const int forceMode = ce_cameratools_autodirector_mode.GetInt();

		if (forceMode == OBS_MODE_FIXED || forceMode == OBS_MODE_IN_EYE || forceMode == OBS_MODE_CHASE || forceMode == OBS_MODE_ROAMING)
			iMode = forceMode;

		GetHooks()->GetOriginal<HookFunc::C_HLTVCamera_SetMode>()(iMode);
		GetHooks()->SetState<HookFunc::C_HLTVCamera_SetMode>(Hooking::HookAction::SUPERCEDE);
	}

	m_SwitchReason = ModeSwitchReason::Unknown;
}

void CameraTools::SetPrimaryTargetOverride(int nEntity)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	const int forceTarget = ce_cameratools_force_target.GetInt();

	if (Interfaces::GetClientEntityList()->GetClientEntity(forceTarget))
		nEntity = forceTarget;

	if (!Interfaces::GetClientEntityList()->GetClientEntity(nEntity))
		nEntity = ((HLTVCameraOverride *)Interfaces::GetHLTVCamera())->m_iTraget1;

	GetHooks()->GetOriginal<HookFunc::C_HLTVCamera_SetPrimaryTarget>()(nEntity);
	GetHooks()->SetState<HookFunc::C_HLTVCamera_SetPrimaryTarget>(Hooking::HookAction::SUPERCEDE);
}

void CameraTools::ChangeForceTarget(IConVar *var, const char *pOldValue, float flOldValue)
{
	const int forceTarget = ce_cameratools_force_target.GetInt();

	if (Interfaces::GetClientEntityList()->GetClientEntity(forceTarget))
	{
		m_SetPrimaryTargetHook.Enable();

		try
		{
			Interfaces::GetHLTVCamera()->SetPrimaryTarget(forceTarget);
		}
		catch (bad_pointer)
		{
			Warning("Error in setting target.\n");
		}
	}
	else
	{
		if (!ce_cameratools_force_valid_target.GetBool())
			m_SetPrimaryTargetHook.Disable();
	}
}

bool CameraTools::ParseSpecPosCommand(const CCommand& command, Vector& pos, QAngle& ang, ObserverMode& mode,
	const Vector& defaultPos, const QAngle& defaultAng, ObserverMode defaultMode) const
{
	if (command.ArgC() < 2 || command.ArgC() > 8)
		goto PrintUsage;

	for (int i = 1; i < 7; i++)
	{
		const auto arg = i < command.ArgC() ? command.Arg(i) : nullptr;

		float& param = i < 4 ? pos[i - 1] : ang[i - 4];

		if (!arg || (arg[0] == '?' && arg[1] == '\0'))
		{
			param = i < 4 ? defaultPos[i - 1] : defaultAng[i - 4];
		}
		else if (!TryParseFloat(arg, param))
		{
			PluginWarning("Invalid parameter \"%s\" (arg %i) for %s command\n", command.Arg(i), i - 1, command.Arg(0));
			goto PrintUsage;
		}
	}

	mode = defaultMode;
	if (command.ArgC() > 7)
	{
		const auto modeArg = command.Arg(7);

		if (!stricmp(modeArg, "fixed"))
			mode = OBS_MODE_FIXED;
		else if (!stricmp(modeArg, "free"))
			mode = OBS_MODE_ROAMING;
		else if (modeArg[0] != '?' || modeArg[1] != '\0')
		{
			PluginWarning("Invalid parameter \"%s\" for mode (expected \"fixed\" or \"free\")\n");
			goto PrintUsage;
		}
	}

	return true;

PrintUsage:
	PluginMsg(
		"Usage: %s x [y] [z] [pitch] [yaw] [roll] [mode]\n"
		"\tIf any of the parameters are '?' or omitted, they are left untouched.\n"
		"\tMode can be either \"fixed\" or \"free\"\n",
		command.Arg(0));
	return false;
}

void CameraTools::SpecPosition(const CCommand &command)
{
	Vector pos;
	QAngle ang;
	ObserverMode mode;

	const auto camState = CameraState::GetModule();
	if (!camState)
	{
		Warning("%s: Failed to get CameraState module\n", command[0]);
		return;
	}

	const ObserverMode defaultMode = (ObserverMode)Interfaces::GetHLTVCamera()->GetMode();
	const auto activeCam = camState->GetActiveCamera();

	// Legacy support, we used to always force OBS_MODE_FIXED
	if (ParseSpecPosCommand(command, pos, ang, mode, activeCam->GetOrigin(), activeCam->GetAngles(), defaultMode))
		SpecPosition(pos, ang, mode);
}

void CameraTools::SpecPositionDelta(const CCommand& command)
{
	Vector pos;
	QAngle ang;
	ObserverMode mode;

	const ObserverMode defaultMode = (ObserverMode)Interfaces::GetHLTVCamera()->GetMode();

	if (ParseSpecPosCommand(command, pos, ang, mode, vec3_origin, vec3_angle, defaultMode))
	{
		const auto camState = CameraState::GetModule();
		if (!camState)
		{
			Warning("%s: Failed to get CameraState module\n", command[0]);
			return;
		}

		const auto activeCam = camState->GetActiveCamera();

		pos += activeCam->GetOrigin();
		ang += activeCam->GetAngles();

		SpecPosition(pos, ang, mode);
	}
}

void CameraTools::ToggleForceValidTarget(IConVar *var, const char *pOldValue, float flOldValue)
{
	if (ce_cameratools_force_valid_target.GetBool())
		m_SetPrimaryTargetHook.Enable();
	else if(!Interfaces::GetClientEntityList()->GetClientEntity(ce_cameratools_force_target.GetInt()))
		m_SetPrimaryTargetHook.Disable();
}

float CameraTools::TPLockValue::GetValue(float input) const
{
	switch (m_Mode)
	{
		case Mode::Set:
			return m_Value;
		case Mode::Add:
			return input + m_Value;
		case Mode::Scale:
			return input * m_Value;
		case Mode::ScaleAdd:
			return input * m_Value + m_Base;
	}

	Assert(!"Should never get here...");
	return NAN;
}
