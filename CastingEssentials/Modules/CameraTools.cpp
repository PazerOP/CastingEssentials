#include "CameraTools.h"
#include "PluginBase/Funcs.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"
#include <filesystem.h>
#include <tier1/KeyValues.h>
#include <tier3/tier3.h>
#include <cdll_int.h>
#include <client/c_baseentity.h>
#include <characterset.h>

CameraTools::CameraTools()
{
	m_SetModeHook = 0;
	m_SetPrimaryTargetHook = 0;
	m_SpecGUISettings = new KeyValues("Resource/UI/SpectatorTournament.res");
	m_SpecGUISettings->LoadFromFile(g_pFullFileSystem, "resource/ui/spectatortournament.res", "mod");

	m_ForceMode = new ConVar("ce_cameratools_force_mode", "0", FCVAR_NONE, "Force the camera mode to this value");
	m_ForceTarget = new ConVar("ce_cameratools_force_target", "-1", FCVAR_NONE, "Force the camera target to this player index");
	m_ForceValidTarget = new ConVar("ce_cameratools_force_valid_target", "0", FCVAR_NONE, "Forces the camera to only have valid targets.");
	m_SpecPlayerAlive = new ConVar("ce_cameratools_spec_player_alive", "1", FCVAR_NONE, "Prevents spectating dead players.");

	m_SpecClass = new ConCommand("ce_cameratools_spec_class", [](const CCommand& command) { Modules().GetModule<CameraTools>()->SpecClass(command); }, "Spectates a specific class. ce_cameratools_spec_class <team> <class> [index]", FCVAR_NONE);
	m_SpecSteamID = new ConCommand("ce_cameratools_spec_steamid", [](const CCommand& command) { Modules().GetModule<CameraTools>()->SpecSteamID(command); }, "Spectates a player with the given steamid. ce_cameratools_spec_steamid <steamID>", FCVAR_NONE);

	m_ShowUsers = new ConCommand("ce_cameratools_show_users", [](const CCommand& command) { Modules().GetModule<CameraTools>()->ShowUsers(command); }, "Lists all currently connected players on the server.", FCVAR_NONE);
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
		Funcs::GetFunc_C_HLTVCamera_SetCameraAngle();
	}
	catch (bad_pointer)
	{
		PluginWarning("Required function C_HLTVCamera::SetCameraAngle for module %s not available!\n", Modules().GetModuleName<CameraTools>().c_str());
		ready = false;
	}

	try
	{
		Funcs::GetFunc_C_HLTVCamera_SetMode();
	}
	catch (bad_pointer)
	{
		PluginWarning("Required function C_HLTVCamera::SetMode for module %s not available!\n", Modules().GetModuleName<CameraTools>().c_str());
		ready = false;
	}

	try
	{
		Funcs::GetFunc_C_HLTVCamera_SetPrimaryTarget();
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

void CameraTools::ShowUsers(const CCommand& command)
{
	Msg("Players:\n");

	int red = 0, blue = 0;
	for (Player* player : Player::Iterable())
	{
		if (player->GetClass() == TFClassType::Unknown)
			continue;

		const char* team;
		int index;
		switch (player->GetTeam())
		{
			case TFTeam::Red:
				index = red++;
				team = "red";
				break;

			case TFTeam::Blue:
				index = blue++;
				team = "blu";
				break;

			default:
				continue;
		}

		Msg("alias player_%s%i \"%s %s\"		// %s\n",
			team, index, m_SpecSteamID->GetName(), RenderSteamID(player->GetSteamID().ConvertToUint64()).c_str(), player->GetName().c_str());
	}
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
				Funcs::GetFunc_C_HLTVCamera_SetPrimaryTarget()(Interfaces::GetHLTVCamera(), player->GetEntity()->entindex());
			}
			catch (bad_pointer &e)
			{
				Warning("%s\n", e.what());
			}
		}
	}
}

void CameraTools::SpecSteamID(const CCommand& command)
{
	characterset_t newSet;
	CharacterSetBuild(&newSet, "{}()'");	// Everything the default set has, minus the ':'
	CCommand newCommand;
	if (!newCommand.Tokenize(command.GetCommandString(), &newSet))
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