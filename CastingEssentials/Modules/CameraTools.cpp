#include "CameraTools.h"
#include "Misc/CCvar.h"
#include "Misc/HLTVCameraHack.h"
#include "Modules/Camera/SimpleCameraSmooth.h"
#include "Modules/CameraSmooths.h"
#include "Modules/CameraState.h"
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

static IClientEntityList* s_ClientEntityList;
static IVEngineClient* s_EngineClient;

CameraTools::CameraTools() :
	ce_cameratools_autodirector_mode("ce_cameratools_autodirector_mode", "0", FCVAR_NONE,
		"Forces the camera mode to this value while spec_autodirector is enabled. 0 = don't force anything"),
	ce_cameratools_force_target("ce_cameratools_force_target", "-1", FCVAR_NONE, "Forces the camera target to this player index."),
	ce_cameratools_disable_view_punches("ce_cameratools_disable_view_punches", "0", FCVAR_NONE,
		"Disables all view punches (used for recoil effects on some weapons)",
		[](IConVar* var, const char*, float) { GetModule()->ToggleDisableViewPunches(static_cast<ConVar*>(var)); }),

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

	ce_cameratools_smoothto("ce_cameratools_smoothto", &SmoothTo,
		"Interpolates between the current camera and a given target: "
		"\"ce_cameratools_smoothto <x> <y> <z> <pitch> <yaw> <roll> [duration] [smooth mode] [force]\"\n"
		"\tSmooth mode can be either linear or smoothstep, default linear."),

	ce_cameratools_show_users("ce_cameratools_show_users", [](const CCommand& args) { GetModule()->ShowUsers(args); },
		"Lists all currently connected players on the server.")
{
}

bool CameraTools::CheckDependencies()
{
	Modules().Depend<CameraState>();

	if (!CheckDependency(Interfaces::GetClientEntityList(), s_ClientEntityList))
		return false;
	if (!CheckDependency(Interfaces::GetEngineClient(), s_EngineClient))
		return false;

	if (!Player::CheckDependencies())
	{
		PluginWarning("Required player helper class for module %s not available!\n", GetModuleName());
		return false;
	}

	return true;
}

void CameraTools::SpecPosition(const Vector& pos, const QAngle& angle, ObserverMode mode, float fov)
{
	if (mode == OBS_MODE_FIXED)
	{
		auto cam = std::make_shared<SimpleCamera>();
		cam->m_Origin = pos;
		cam->m_Angles = angle;
		cam->m_FOV = fov == INFINITY ? 90 : fov;
		cam->m_Type = CameraType::Fixed;

		CameraState::GetModule()->SetCamera(cam);
	}
	else if (mode == OBS_MODE_ROAMING)
	{
		auto cam = std::make_shared<RoamingCamera>();
		cam->SetPosition(pos, angle);

		if (fov != INFINITY)
			cam->SetFOV(fov);

		CameraState::GetModule()->SetCamera(cam);
	}
	else
	{
		Warning("%s: Programmer error! mode was %i\n", __FUNCTION__, mode);
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

void CameraTools::GetSmoothTestSettings(const std::string_view& testsString, SmoothSettings& settings)
{
	if (stristr(testsString, "all"sv) != testsString.end())
	{
		settings.m_TestFOV = true;
		settings.m_TestDist = true;
		settings.m_TestCooldown = true;
		settings.m_TestLOS = true;
	}
	else
	{
		settings.m_TestFOV = stristr(testsString, "fov"sv) != testsString.end();
		settings.m_TestDist = stristr(testsString, "dist"sv) != testsString.end();
		settings.m_TestCooldown = stristr(testsString, "time"sv) != testsString.end();
		settings.m_TestLOS = stristr(testsString, "los"sv) != testsString.end();
	}
}

void CameraTools::SpecModeChanged(ObserverMode oldMode, ObserverMode& newMode)
{
	static ConVarRef spec_autodirector("spec_autodirector");
	if (!spec_autodirector.GetBool())
		return;

	const auto forceMode = ce_cameratools_autodirector_mode.GetInt();

	switch (forceMode)
	{
		case OBS_MODE_NONE:
			return;

		case OBS_MODE_FIXED:
		case OBS_MODE_IN_EYE:
		case OBS_MODE_CHASE:
		case OBS_MODE_ROAMING:
			newMode = (ObserverMode)forceMode;
			break;

		default:
			Warning("Unknown/unsupported value %i for %s\n", forceMode, ce_cameratools_autodirector_mode.GetName());
	}
}

void CameraTools::SpecTargetChanged(IClientEntity* oldEnt, IClientEntity*& newEnt)
{
	if (auto ent = s_ClientEntityList->GetClientEntity(ce_cameratools_force_target.GetInt()))
		newEnt = ent;
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
		auto localMode = CameraState::GetModule()->GetLocalObserverMode();
		if (localMode == OBS_MODE_FIXED ||
			localMode == OBS_MODE_IN_EYE ||
			localMode == OBS_MODE_CHASE)
		{
			Player* spectatingPlayer = Player::AsPlayer(CameraState::GetModule()->GetLocalObserverTarget());
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

	if (!player->IsAlive())
	{
		DevWarning("%s: Not speccing \"%s\" because they are dead (%s)\n", __FUNCTION__, player->GetName());
		return;
	}

	char buf[32];
	sprintf_s(buf, "spec_player \"#%i\"", player->GetUserID());
	s_EngineClient->ClientCmd(buf);
}

void CameraTools::SmoothTo(const CCommand& cmd)
{
	auto cs = CameraState::GetModule();
	if (!cs)
	{
		Warning("%s: Unable to get Camera State module\n", cmd[0]);
		return;
	}

	do
	{
		if (cmd.ArgC() < 1 || cmd.ArgC() > 10)
			break;

		const auto& activeCam = cs->GetCurrentCamera();
		if (!activeCam)
		{
			Warning("%s: No current active camera?\n", cmd[0]);
			break;
		}

		const auto& currentPos = activeCam->GetOrigin();
		const auto& currentAng = activeCam->GetAngles();

		auto startCam = std::make_shared<SimpleCamera>();
		{
			startCam->m_Angles = activeCam->GetAngles();
			startCam->m_Origin = activeCam->GetOrigin();
			startCam->m_FOV = activeCam->GetFOV();
		}

		Vector endCamPos;
		QAngle endCamAng;
		for (uint_fast8_t i = 0; i < 6; i++)
		{
			const auto& arg = cmd.ArgC() > i + 1 ? cmd[i + 1] : "?";
			auto& param = i < 3 ? endCamPos[i] : endCamAng[i - 3];

			if (arg[0] == '?' && arg[1] == '\0')
				param = i < 3 ? currentPos[i] : currentAng[i - 3];
			else if (!TryParseFloat(arg, param))
			{
				Warning("%s: Unable to parse \"%s\" (arg %i) as a float\n", cmd[0], arg, i);
				break;
			}
		}

		SmoothSettings settings;

		if (cmd.ArgC() > 7 && !TryParseFloat(cmd[7], settings.m_DurationOverride))
		{
			Warning("%s: Unable to parse arg 7 (duration, \"%s\") as a float\n", cmd[0], cmd[7]);
			break;
		}

		if (cmd.ArgC() > 8 && (cmd[8][0] != '?' || cmd[8][1] != '\0'))
		{
			if (!stricmp(cmd[8], "linear"))
				settings.m_InterpolatorOverride = &Interpolators::Linear;
			else if (!stricmp(cmd[8], "smoothstep"))
				settings.m_InterpolatorOverride = &Interpolators::Smoothstep;
			else
				Warning("%s: Unrecognized smooth mode %s, defaulting to linear\n", cmd[0], cmd[8]);
		}

		if (cmd.ArgC() > 9)
			GetSmoothTestSettings(cmd[9], settings);
		else
		{
			settings.m_TestFOV = false;
			settings.m_TestCooldown = false;
		}

		auto endCam = std::make_shared<RoamingCamera>();
		endCam->SetInputEnabled(false);
		endCam->SetPosition(endCamPos, endCamAng);
		cs->SetCameraSmoothed(endCam, settings);

		return;

	} while (false);

	Warning("Usage: %s [x] [y] [z] [pitch] [yaw] [roll] [duration] [smooth mode] [tests]\n"
		"\tSmooth mode can be either linear or smoothstep.\n"
		"\tIf any of the pos/angle parameters are '?' or omitted, they are left untouched.\n"
		"\t'?' or omission for duration and smooth mode means use ce_smoothing_ cvars.\n"
		"\ttests: default 'dist+los'. Can be 'all', 'none', or combined with '+' (NO SPACES):\n"
		"\t\tlos:  Don't smooth if we don't have LOS to the target (ce_smoothing_los_)\n"
		"\t\tdist: Don't smooth if we're too far away (ce_smoothing_*_distance)\n"
		"\t\ttime: Don't smooth if we're still on cooldown (ce_smoothing_cooldown)\n"
		"\t\tfov:  Don't smooth if target is outside of fov (ce_smoothing_fov)\n"
		, cmd[0]);
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
	Player* allPlayers[MAX_PLAYERS];
	const auto endIter = Player::GetSortedPlayers(team, std::begin(allPlayers), std::end(allPlayers));

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

	SpecPlayer(allPlayers[index]->entindex());
	return;

Usage:
	PluginWarning("Usage: %s <red/blue> <index>\n", command.Arg(0));
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

void CameraTools::SpecPosition(const CCommand& command)
{
	const auto camState = CameraState::GetModule();
	if (!camState)
	{
		Warning("%s: Failed to get CameraState module\n", command[0]);
		return;
	}

	ObserverMode defaultMode = camState->GetDesiredObserverMode();
	if (defaultMode != OBS_MODE_FIXED && defaultMode != OBS_MODE_ROAMING)
		defaultMode = OBS_MODE_ROAMING;

	const auto activeCam = camState->GetCurrentCamera();
	if (!activeCam)
	{
		Warning("%s: No active camera?\n", command[0]);
		return;
	}

	Vector pos;
	QAngle ang;
	ObserverMode mode;
	if (ParseSpecPosCommand(command, pos, ang, mode, activeCam->GetOrigin(), activeCam->GetAngles(), defaultMode))
		SpecPosition(pos, ang, mode);
}

void CameraTools::SpecPositionDelta(const CCommand& command)
{
	const auto camState = CameraState::GetModule();
	if (!camState)
	{
		Warning("%s: Failed to get CameraState module\n", command[0]);
		return;
	}

	ObserverMode defaultMode = camState->GetDesiredObserverMode();
	if (defaultMode != OBS_MODE_FIXED && defaultMode != OBS_MODE_ROAMING)
		defaultMode = OBS_MODE_ROAMING;

	const auto activeCam = camState->GetCurrentCamera();
	if (!activeCam)
	{
		Warning("%s: No active camera?\n", command[0]);
		return;
	}

	Vector pos;
	QAngle ang;
	ObserverMode mode;
	if (ParseSpecPosCommand(command, pos, ang, mode, vec3_origin, vec3_angle, defaultMode))
	{
		pos += activeCam->GetOrigin();
		ang += activeCam->GetAngles();

		SpecPosition(pos, ang, mode);
	}
}
