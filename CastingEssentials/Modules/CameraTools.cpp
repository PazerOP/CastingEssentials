#include "CameraTools.h"
#include "Misc/HLTVCameraHack.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"
#include <filesystem.h>
#include <tier1/KeyValues.h>
#include <tier3/tier3.h>
#include <cdll_int.h>
#include <client/c_baseentity.h>
#include <characterset.h>
#include <functional>
#include <client/hltvcamera.h>
#include <toolframework/ienginetool.h>

CameraTools::CameraTools()
{
	m_SetModeHook = 0;
	m_SetPrimaryTargetHook = 0;
	m_SpecGUISettings = new KeyValues("Resource/UI/SpectatorTournament.res");
	m_SpecGUISettings->LoadFromFile(g_pFullFileSystem, "resource/ui/spectatortournament.res", "mod");

	ce_cameratools_show_mode = new ConVar("ce_cameratools_show_mode", "0", FCVAR_NONE, "Displays the current spec_mode in the top right corner of the screen.");
	m_ForceMode = new ConVar("ce_cameratools_force_mode", "0", FCVAR_NONE, "Forces the camera mode to this value.", [](IConVar* var, const char* pOldValue, float flOldValue) { GetModule()->ChangeForceMode(var, pOldValue, flOldValue); });
	m_ForceTarget = new ConVar("ce_cameratools_force_target", "-1", FCVAR_NONE, "Forces the camera target to this player index.", [](IConVar* var, const char* pOldValue, float flOldValue) { GetModule()->ChangeForceTarget(var, pOldValue, flOldValue); });
	m_ForceValidTarget = new ConVar("ce_cameratools_force_valid_target", "0", FCVAR_NONE, "Forces the camera to only have valid targets.", [](IConVar* var, const char* pOldValue, float flOldValue) { GetModule()->ToggleForceValidTarget(var, pOldValue, flOldValue); });
	m_SpecPlayerAlive = new ConVar("ce_cameratools_spec_player_alive", "1", FCVAR_NONE, "Prevents spectating dead players.");

	m_SpecPosition = new ConCommand("ce_cameratools_spec_pos", [](const CCommand& args) { GetModule()->SpecPosition(args); }, "Moves the camera to a given position.");

	m_SpecClass = new ConCommand("ce_cameratools_spec_class", [](const CCommand& args) { GetModule()->SpecClass(args); }, "Spectates a specific class: ce_cameratools_spec_class <team> <class> [index]");
	m_SpecSteamID = new ConCommand("ce_cameratools_spec_steamid", [](const CCommand& args) { GetModule()->SpecSteamID(args); }, "Spectates a player with the given steamid: ce_cameratools_spec_steamid <steamID>");

	m_ShowUsers = new ConCommand("ce_cameratools_show_users", [](const CCommand& args) { GetModule()->ShowUsers(args); }, "Lists all currently connected players on the server.");
}

bool CameraTools::CheckDependencies()
{
	bool ready = true;

	if (!Interfaces::GetEngineTool())
	{
		PluginWarning("Required interface IEngineTool for module %s not available!\n", Modules().GetModuleName<CameraTools>().c_str());
		ready = false;
	}

	if (!g_pFullFileSystem)
	{
		PluginWarning("Required interface IFileSystem for module %s not available!\n", Modules().GetModuleName<CameraTools>().c_str());
		ready = false;
	}

	try
	{
		GetHooks()->GetFunc<C_HLTVCamera_SetCameraAngle>();
	}
	catch (bad_pointer)
	{
		PluginWarning("Required function C_HLTVCamera::SetCameraAngle for module %s not available!\n", Modules().GetModuleName<CameraTools>().c_str());
		ready = false;
	}

	try
	{
		GetHooks()->GetFunc<C_HLTVCamera_SetMode>();
	}
	catch (bad_pointer)
	{
		PluginWarning("Required function C_HLTVCamera::SetMode for module %s not available!\n", Modules().GetModuleName<CameraTools>().c_str());
		ready = false;
	}

	try
	{
		GetHooks()->GetFunc<C_HLTVCamera_SetPrimaryTarget>();
	}
	catch (bad_pointer)
	{
		PluginWarning("Required function C_HLTVCamera::SetPrimaryTarget for module %s not available!\n", Modules().GetModuleName<CameraTools>().c_str());
		ready = false;
	}

	if (!g_pVGuiPanel)
	{
		PluginWarning("Required interface vgui::IPanel for module %s not available!\n", Modules().GetModuleName<CameraTools>().c_str());
		ready = false;
	}

	if (!g_pVGuiSchemeManager)
	{
		PluginWarning("Required interface vgui::ISchemeManager for module %s not available!\n", Modules().GetModuleName<CameraTools>().c_str());
		ready = false;
	}

	if (!Player::CheckDependencies())
	{
		PluginWarning("Required player helper class for module %s not available!\n", Modules().GetModuleName<CameraTools>().c_str());
		ready = false;
	}

	if (!Player::IsNameRetrievalAvailable())
	{
		PluginWarning("Required player name retrieval for module %s not available!\n", Modules().GetModuleName<CameraTools>().c_str());
		ready = false;
	}

	try
	{
		Interfaces::GetClientMode();
	}
	catch (bad_pointer)
	{
		PluginWarning("Module %s requires IClientMode, which cannot be verified at this time!\n", Modules().GetModuleName<CameraTools>().c_str());
	}

	try
	{
		Interfaces::GetHLTVCamera();
	}
	catch (bad_pointer)
	{
		PluginWarning("Module %s requires C_HLTVCamera, which cannot be verified at this time!\n", Modules().GetModuleName<CameraTools>().c_str());
	}

	return ready;
}

void CameraTools::SpecPosition(const Vector& pos, const QAngle& angle)
{
	try
	{
		HLTVCameraOverride *hltvcamera = Interfaces::GetHLTVCamera();

		hltvcamera->m_nCameraMode = OBS_MODE_FIXED;
		hltvcamera->m_iCameraMan = 0;
		hltvcamera->m_vCamOrigin = pos;
		hltvcamera->m_aCamAngle = angle;
		hltvcamera->m_iTraget1 = 0;
		hltvcamera->m_iTraget2 = 0;
		hltvcamera->m_flLastAngleUpdateTime = -1;

		GetHooks()->GetFunc<C_HLTVCamera_SetCameraAngle>()(hltvcamera->m_aCamAngle);
	}
	catch (bad_pointer &e)
	{
		Warning("%s\n", e.what());
	}
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

	for (size_t i = 0; i < blu.size(); i++)
		ConColorMsg(Color(128, 128, 255, 255), "    alias player_blu%i \"%s %s\"		// %s\n", i, m_SpecSteamID->GetName(), RenderSteamID(blu[i]->GetSteamID().ConvertToUint64()).c_str(), blu[i]->GetName().c_str());

	for (size_t i = 0; i < red.size(); i++)
		ConColorMsg(Color(255, 128, 128, 255), "    alias player_red%i \"%s %s\"		// %s\n", i, m_SpecSteamID->GetName(), RenderSteamID(red[i]->GetSteamID().ConvertToUint64()).c_str(), red[i]->GetName().c_str());
}

void CameraTools::SpecClass(const CCommand& command)
{
	// Usage: <team> <class> [classIndex]
	if (command.ArgC() < 3 || command.ArgC() > 4)
	{
		PluginWarning("%s: Expected either 2 or 3 arguments\n", m_SpecClass->GetName());
		goto Usage;
	}

	TFTeam team;
	if (!strnicmp(command.Arg(1), "blu", 3))
		team = TFTeam::Blue;
	else if (!strnicmp(command.Arg(1), "red", 3))
		team = TFTeam::Red;
	else
	{
		PluginWarning("%s: Unknown team \"%s\"\n", m_SpecClass->GetName(), command.Arg(1));
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
	else if (!strnicmp(command.Arg(2), "heavy", 5))
		playerClass = TFClassType::Heavy;
	else if (!stricmp(command.Arg(2), "engineer") || !stricmp(command.Arg(2), "engie"))
		playerClass = TFClassType::Engineer;
	else if (!stricmp(command.Arg(2), "medic"))
		playerClass = TFClassType::Medic;
	else if (!stricmp(command.Arg(2), "sniper"))
		playerClass = TFClassType::Sniper;
	else if (!stricmp(command.Arg(2), "spy"))
		playerClass = TFClassType::Spy;
	else
	{
		PluginWarning("%s: Unknown class \"%s\"\n", m_SpecClass->GetName(), command.Arg(2));
		goto Usage;
	}

	int classIndex = -1;
	if (command.ArgC() > 3)
	{
		if (!IsInteger(command.Arg(3)))
		{
			PluginWarning("%s: class index \"%s\" is not an integer\n", m_SpecClass->GetName(), command.Arg(3));
			goto Usage;
		}

		classIndex = atoi(command.Arg(3));
	}

	SpecClass(team, playerClass, classIndex);
	return;

Usage:
	PluginWarning("Usage: %s\n", m_SpecClass->GetHelpText());
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
		Player* localPlayer = Player::GetPlayer(Interfaces::GetEngineClient()->GetLocalPlayer(), __FUNCSIG__);
		if (!localPlayer)
			return;

		if (localPlayer->GetObserverMode() == OBS_MODE_FIXED ||
			localPlayer->GetObserverMode() == OBS_MODE_IN_EYE ||
			localPlayer->GetObserverMode() == OBS_MODE_CHASE)
		{
			Player* spectatingPlayer = Player::AsPlayer(localPlayer->GetObserverTarget());
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

void CameraTools::SpecPlayer(int playerIndex)
{
	Player* player = Player::GetPlayer(playerIndex, __FUNCSIG__);

	if (player)
	{
		if (!m_SpecPlayerAlive->GetBool() || player->IsAlive())
		{
			try
			{
				GetHooks()->GetFunc<C_HLTVCamera_SetPrimaryTarget>()(player->GetEntity()->entindex());
			}
			catch (bad_pointer &e)
			{
				Warning("%s\n", e.what());
			}
		}
	}
}

void CameraTools::OnTick(bool inGame)
{
	if (inGame)
	{
		if (ce_cameratools_show_mode->GetBool())
		{
			int mode = -1;
			if (Interfaces::GetEngineClient()->IsHLTV())
				mode = Interfaces::GetHLTVCamera()->m_nCameraMode;
			else
			{
				Player* local = Player::GetLocalPlayer();
				if (local)
					mode = local->GetObserverMode();
			}

			if (mode >= 0)
			{
				Interfaces::GetEngineClient()->Con_NPrintf(0, "Current spec_mode: %i %s",
					mode, mode >= 0 && mode < NUM_OBSERVER_MODES ? s_ObserverModes[mode] : "INVALID");
			}
		}
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
		PluginWarning("%s: Expected 1 argument\n", m_SpecSteamID->GetName());
		goto Usage;
	}

	parsed = ParseSteamID(newCommand.Arg(1));
	if (!parsed.IsValid())
	{
		PluginWarning("%s: Unable to parse steamid\n", m_SpecSteamID->GetName());
		goto Usage;
	}

	for (Player* player : Player::Iterable())
	{
		if (player->GetSteamID() == parsed)
			SpecPlayer(player->GetEntity()->entindex());
	}

	return;

Usage:
	PluginWarning("Usage: %s\n", m_SpecSteamID->GetHelpText());
}

void CameraTools::ChangeForceMode(IConVar *var, const char *pOldValue, float flOldValue)
{
	const int forceMode = m_ForceMode->GetInt();

	if (forceMode == OBS_MODE_FIXED || forceMode == OBS_MODE_IN_EYE || forceMode == OBS_MODE_CHASE || forceMode == OBS_MODE_ROAMING)
	{
		if (!m_SetModeHook)
			m_SetModeHook = GetHooks()->AddHook<C_HLTVCamera_SetMode>(
				std::bind(&CameraTools::SetModeOverride, this, std::placeholders::_1));

		try
		{
			GetHooks()->GetHook<C_HLTVCamera_SetMode>()->GetOriginal()(forceMode);
		}
		catch (bad_pointer)
		{
			Warning("Error in setting mode.\n");
		}
	}
	else
	{
		var->SetValue(OBS_MODE_NONE);

		if (m_SetModeHook)
		{
			GetHooks()->RemoveHook<C_HLTVCamera_SetMode>(m_SetModeHook, __FUNCSIG__);
			m_SetModeHook = 0;
		}
	}
}

void CameraTools::SetModeOverride(int iMode)
{
	const int forceMode = m_ForceMode->GetInt();

	if (forceMode == OBS_MODE_FIXED || forceMode == OBS_MODE_IN_EYE || forceMode == OBS_MODE_CHASE || forceMode == OBS_MODE_ROAMING)
		iMode = forceMode;

	GetHooks()->GetOriginal<C_HLTVCamera_SetMode>()(iMode);
	GetHooks()->GetHook<C_HLTVCamera_SetMode>()->SetState(Hooking::HookAction::SUPERCEDE);
}

void CameraTools::SetPrimaryTargetOverride(int nEntity)
{
	const int forceTarget = m_ForceTarget->GetInt();

	if (Interfaces::GetClientEntityList()->GetClientEntity(forceTarget))
		nEntity = forceTarget;

	if (!Interfaces::GetClientEntityList()->GetClientEntity(nEntity))
		nEntity = ((HLTVCameraOverride *)Interfaces::GetHLTVCamera())->m_iTraget1;

	GetHooks()->GetOriginal<C_HLTVCamera_SetPrimaryTarget>()(nEntity);
	GetHooks()->GetHook<C_HLTVCamera_SetPrimaryTarget>()->SetState(Hooking::HookAction::SUPERCEDE);
}

void CameraTools::ChangeForceTarget(IConVar *var, const char *pOldValue, float flOldValue)
{
	const int forceTarget = m_ForceTarget->GetInt();

	if (Interfaces::GetClientEntityList()->GetClientEntity(forceTarget))
	{
		if (!m_SetPrimaryTargetHook)
			m_SetPrimaryTargetHook = GetHooks()->AddHook<C_HLTVCamera_SetPrimaryTarget>(
				std::bind(&CameraTools::SetPrimaryTargetOverride, this, std::placeholders::_1));

		try
		{
			GetHooks()->GetFunc<C_HLTVCamera_SetPrimaryTarget>()(forceTarget);
		}
		catch (bad_pointer)
		{
			Warning("Error in setting target.\n");
		}
	}
	else
	{
		if (!m_ForceValidTarget->GetBool())
		{
			if (m_SetPrimaryTargetHook)
			{
				GetHooks()->RemoveHook<C_HLTVCamera_SetPrimaryTarget>(m_SetPrimaryTargetHook, __FUNCSIG__);
				m_SetPrimaryTargetHook = 0;
			}
		}
	}
}

void CameraTools::SpecPosition(const CCommand &command)
{
	if (command.ArgC() >= 6 && IsFloat(command.Arg(1)) && IsFloat(command.Arg(2)) && IsFloat(command.Arg(3)) && IsFloat(command.Arg(4)) && IsFloat(command.Arg(5)))
	{
		SpecPosition(
			Vector(atof(command.Arg(1)), atof(command.Arg(2)), atof(command.Arg(3))),
			QAngle(atof(command.Arg(4)), atof(command.Arg(5)), 0));
	}
	else
	{
		PluginWarning("Usage: statusspec_cameratools_spec_pos <x> <y> <z> <yaw> <pitch>\n");
		return;
	}
}

void CameraTools::ToggleForceValidTarget(IConVar *var, const char *pOldValue, float flOldValue)
{
	if (m_ForceValidTarget->GetBool())
	{
		if (!m_SetPrimaryTargetHook)
			m_SetPrimaryTargetHook = GetHooks()->GetHook<C_HLTVCamera_SetPrimaryTarget>()->AddHook(
				std::bind(&CameraTools::SetPrimaryTargetOverride, this, std::placeholders::_1));
	}
	else
	{
		if (!Interfaces::GetClientEntityList()->GetClientEntity(m_ForceTarget->GetInt()))
		{
			if (m_SetPrimaryTargetHook && GetHooks()->GetHook<C_HLTVCamera_SetPrimaryTarget>()->RemoveHook(m_SetPrimaryTargetHook, __FUNCSIG__))
				m_SetPrimaryTargetHook = 0;

			Assert(!m_SetPrimaryTargetHook);
		}
	}
}