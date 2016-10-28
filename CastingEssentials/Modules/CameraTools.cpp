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
#include <functional>
#include <client/hltvcamera.h>
#include <toolframework/ienginetool.h>

class HLTVCameraOverride : public C_HLTVCamera
{
public:
	using C_HLTVCamera::m_nCameraMode;
	using C_HLTVCamera::m_iCameraMan;
	using C_HLTVCamera::m_vCamOrigin;
	using C_HLTVCamera::m_aCamAngle;
	using C_HLTVCamera::m_iTraget1;
	using C_HLTVCamera::m_iTraget2;
	using C_HLTVCamera::m_flFOV;
	using C_HLTVCamera::m_flOffset;
	using C_HLTVCamera::m_flDistance;
	using C_HLTVCamera::m_flLastDistance;
	using C_HLTVCamera::m_flTheta;
	using C_HLTVCamera::m_flPhi;
	using C_HLTVCamera::m_flInertia;
	using C_HLTVCamera::m_flLastAngleUpdateTime;
	using C_HLTVCamera::m_bEntityPacketReceived;
	using C_HLTVCamera::m_nNumSpectators;
	using C_HLTVCamera::m_szTitleText;
	using C_HLTVCamera::m_LastCmd;
	using C_HLTVCamera::m_vecVelocity;
};

CameraTools::CameraTools()
{
	m_SetModeHook = 0;
	m_SetPrimaryTargetHook = 0;
	m_SpecGUISettings = new KeyValues("Resource/UI/SpectatorTournament.res");
	m_SpecGUISettings->LoadFromFile(g_pFullFileSystem, "resource/ui/spectatortournament.res", "mod");

	m_ForceMode = new ConVar("ce_cameratools_force_mode", "0", FCVAR_NONE, "Forces the camera mode to this value.", &CameraTools::StaticChangeForceMode);
	m_ForceTarget = new ConVar("ce_cameratools_force_target", "-1", FCVAR_NONE, "Forces the camera target to this player index.", &CameraTools::StaticChangeForceTarget);
	m_ForceValidTarget = new ConVar("ce_cameratools_force_valid_target", "0", FCVAR_NONE, "Forces the camera to only have valid targets.", &CameraTools::StaticToggleForceValidTarget);
	m_SpecPlayerAlive = new ConVar("ce_cameratools_spec_player_alive", "1", FCVAR_NONE, "Prevents spectating dead players.");

	m_SpecPosition = new ConCommand("ce_cameratools_spec_pos", &CameraTools::StaticSpecPosition, "Moves the camera to a given position.");

	m_SpecClass = new ConCommand("ce_cameratools_spec_class", &CameraTools::StaticSpecClass, "Spectates a specific class: ce_cameratools_spec_class <team> <class> [index]");
	m_SpecSteamID = new ConCommand("ce_cameratools_spec_steamid", &CameraTools::StaticSpecSteamID, "Spectates a player with the given steamid: ce_cameratools_spec_steamid <steamID>");

	m_ShowUsers = new ConCommand("ce_cameratools_show_users", &CameraTools::StaticShowUsers, "Lists all currently connected players on the server.");
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
				Funcs::GetFunc_C_HLTVCamera_SetPrimaryTarget()(player->GetEntity()->entindex());
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

void CameraTools::StaticShowUsers(const CCommand& command)
{
	CameraTools* module = GetModule();
	if (!module)
	{
		PluginWarning("Unable to use %s: module not loaded\n", command.Arg(0));
		return;
	}

	module->ShowUsers(command);
}

void CameraTools::StaticChangeForceMode(IConVar* var, const char* oldValue, float fOldValue)
{
	CameraTools* module = GetModule();
	if (!module)
	{
		PluginWarning("Unable to use %s: module not loaded\n", var->GetName());
		return;
	}

	module->ChangeForceMode(var, oldValue, fOldValue);
}

void CameraTools::StaticChangeForceTarget(IConVar* var, const char* oldValue, float fOldValue)
{
	CameraTools* module = GetModule();
	if (!module)
	{
		PluginWarning("Unable to use %s: module not loaded\n", var->GetName());
		return;
	}

	module->ChangeForceTarget(var, oldValue, fOldValue);
}

void CameraTools::StaticSpecPosition(const CCommand& command)
{
	CameraTools* module = GetModule();
	if (!module)
	{
		PluginWarning("Unable to use %s: module not loaded\n", command.Arg(0));
		return;
	}

	module->SpecPosition(command);
}

void CameraTools::StaticToggleForceValidTarget(IConVar* var, const char* oldValue, float fOldValue)
{
	CameraTools* module = GetModule();
	if (!module)
	{
		PluginWarning("Unable to use %s: module not loaded\n", var->GetName());
		return;
	}

	module->ToggleForceValidTarget(var, oldValue, fOldValue);
}

void CameraTools::StaticSpecClass(const CCommand& command)
{
	CameraTools* module = GetModule();
	if (!module)
	{
		PluginWarning("Unable to use %s: module not loaded\n", command.Arg(0));
		return;
	}

	module->SpecClass(command);
}

void CameraTools::StaticSpecSteamID(const CCommand& command)
{
	CameraTools* module = GetModule();
	if (!module)
	{
		PluginWarning("Unable to use %s: module not loaded\n", command.Arg(0));
		return;
	}

	module->SpecSteamID(command);
}

void CameraTools::ChangeForceMode(IConVar *var, const char *pOldValue, float flOldValue)
{
	const int forceMode = m_ForceMode->GetInt();

	if (forceMode == OBS_MODE_FIXED || forceMode == OBS_MODE_IN_EYE || forceMode == OBS_MODE_CHASE || forceMode == OBS_MODE_ROAMING)
	{
		if (!m_SetModeHook)
			m_SetModeHook = Funcs::GetHook_C_HLTVCamera_SetMode()->AddHook(
				std::bind(&CameraTools::SetModeOverride, this, std::placeholders::_1));

		try
		{
			Funcs::GetHook_C_HLTVCamera_SetMode()->GetOriginal()(forceMode);
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
			Funcs::GetHook_C_HLTVCamera_SetMode()->RemoveHook(m_SetModeHook, __FUNCSIG__);
			m_SetModeHook = 0;
		}
	}
}

void CameraTools::SetModeOverride(int iMode)
{
	const int forceMode = m_ForceMode->GetInt();

	if (forceMode == OBS_MODE_FIXED || forceMode == OBS_MODE_IN_EYE || forceMode == OBS_MODE_CHASE || forceMode == OBS_MODE_ROAMING)
		iMode = forceMode;

	Funcs::GetHook_C_HLTVCamera_SetMode()->GetOriginal()(iMode);
}

void CameraTools::SetPrimaryTargetOverride(int nEntity)
{
	const int forceTarget = m_ForceTarget->GetInt();

	if (Interfaces::GetClientEntityList()->GetClientEntity(forceTarget))
		nEntity = forceTarget;

	if (!Interfaces::GetClientEntityList()->GetClientEntity(nEntity))
		nEntity = ((HLTVCameraOverride *)Interfaces::GetHLTVCamera())->m_iTraget1;

	Funcs::GetHook_C_HLTVCamera_SetPrimaryTarget()->GetOriginal()(nEntity);
}

void CameraTools::ChangeForceTarget(IConVar *var, const char *pOldValue, float flOldValue)
{
	const int forceTarget = m_ForceTarget->GetInt();

	if (Interfaces::GetClientEntityList()->GetClientEntity(forceTarget))
	{
		if (!m_SetPrimaryTargetHook)
			m_SetPrimaryTargetHook = Funcs::GetHook_C_HLTVCamera_SetPrimaryTarget()->AddHook(
				std::bind(&CameraTools::SetPrimaryTargetOverride, this, std::placeholders::_1));

		try
		{
			Funcs::GetHook_C_HLTVCamera_SetPrimaryTarget()->GetOriginal()(forceTarget);
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
				Funcs::GetHook_C_HLTVCamera_SetPrimaryTarget()->RemoveHook(m_SetPrimaryTargetHook, __FUNCSIG__);
				m_SetPrimaryTargetHook = 0;
			}
		}
	}
}

void CameraTools::SpecPosition(const CCommand &command)
{
	if (command.ArgC() >= 6 && IsFloat(command.Arg(1)) && IsFloat(command.Arg(2)) && IsFloat(command.Arg(3)) && IsFloat(command.Arg(4)) && IsFloat(command.Arg(5)))
	{
		try
		{
			HLTVCameraOverride *hltvcamera = (HLTVCameraOverride *)Interfaces::GetHLTVCamera();

			hltvcamera->m_nCameraMode = OBS_MODE_FIXED;
			hltvcamera->m_iCameraMan = 0;
			hltvcamera->m_vCamOrigin.x = atof(command.Arg(1));
			hltvcamera->m_vCamOrigin.y = atof(command.Arg(2));
			hltvcamera->m_vCamOrigin.z = atof(command.Arg(3));
			hltvcamera->m_aCamAngle.x = atof(command.Arg(4));
			hltvcamera->m_aCamAngle.y = atof(command.Arg(5));
			hltvcamera->m_iTraget1 = 0;
			hltvcamera->m_iTraget2 = 0;
			hltvcamera->m_flLastAngleUpdateTime = Interfaces::GetEngineTool()->GetRealTime();

			Funcs::GetFunc_C_HLTVCamera_SetCameraAngle()(hltvcamera->m_aCamAngle);
		}
		catch (bad_pointer &e)
		{
			Warning("%s\n", e.what());
		}
	}
	else
	{
		Warning("Usage: statusspec_cameratools_spec_pos <x> <y> <z> <yaw> <pitch>\n");

		return;
	}
}

void CameraTools::ToggleForceValidTarget(IConVar *var, const char *pOldValue, float flOldValue)
{
	if (m_ForceValidTarget->GetBool())
	{
		if (!m_SetPrimaryTargetHook)
			m_SetPrimaryTargetHook = Funcs::GetHook_C_HLTVCamera_SetPrimaryTarget()->AddHook(
				std::bind(&CameraTools::SetPrimaryTargetOverride, this, std::placeholders::_1));
	}
	else
	{
		if (!Interfaces::GetClientEntityList()->GetClientEntity(m_ForceTarget->GetInt()))
		{
			if (m_SetPrimaryTargetHook)
			{
				Funcs::GetHook_C_HLTVCamera_SetPrimaryTarget()->RemoveHook(m_SetPrimaryTargetHook, __FUNCSIG__);
				m_SetPrimaryTargetHook = 0;
			}
		}
	}
}