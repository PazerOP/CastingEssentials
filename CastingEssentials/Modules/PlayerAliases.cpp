#include "PlayerAliases.h"
#include "PluginBase/Funcs.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"
#include <convar.h>
#include <steam/steam_api.h>
#include <cdll_int.h>
#include <toolframework/ienginetool.h>

PlayerAliases::PlayerAliases()
{
	m_GetPlayerInfoHook = 0;

	m_Enabled = new ConVar("ce_playeraliases_enabled", "0", FCVAR_NONE, "Enables player aliases.", &PlayerAliases::StaticToggleEnabled);
	m_FormatBlue = new ConVar("ce_playeraliases_format_blu", "%alias%", FCVAR_NONE, "Name format for BLU players.");
	m_FormatRed = new ConVar("ce_playeraliases_format_red", "%alias%", FCVAR_NONE, "Name format for RED players.");
}

bool PlayerAliases::CheckDependencies()
{
	bool ready = true;

	if (!Interfaces::GetEngineClient())
	{
		PluginWarning("Required interface IVEngineClient for module %s not available!\n", Modules().GetModuleName<PlayerAliases>().c_str());
		ready = false;
	}

	if (!Interfaces::AreSteamLibrariesAvailable())
	{
		PluginWarning("Required Steam libraries for module %s not available!\n", Modules().GetModuleName<PlayerAliases>().c_str());
		ready = false;
	}

	if (!Player::CheckDependencies())
	{
		PluginWarning("Required player helper class for module %s not available!\n", Modules().GetModuleName<PlayerAliases>().c_str());
		ready = false;
	}

	if (!Player::IsSteamIDRetrievalAvailable())
	{
		PluginWarning("Required player Steam ID retrieval for module %s not available!\n", Modules().GetModuleName<PlayerAliases>().c_str());
		ready = false;
	}

	return ready;
}

void PlayerAliases::StaticToggleEnabled(IConVar* var, const char* oldValue, float fOldValue)
{
	PlayerAliases* module = GetModule();
	if (!module)
	{
		PluginWarning("Unable to use %s: module not loaded\n", var->GetName());
		return;
	}

	module->ToggleEnabled(var, oldValue, fOldValue);
}

bool PlayerAliases::GetPlayerInfoOverride(int ent_num, player_info_s *pinfo)
{
	bool result = Funcs::GetHook_IVEngineClient_GetPlayerInfo()->GetOriginal()(ent_num, pinfo);

	if (ent_num < 1 || ent_num >= Interfaces::GetEngineTool()->GetMaxClients())
		return result;

	Player* player = Player::GetPlayer(ent_num, __FUNCSIG__);
	if (!player)
		return result;

	static EUniverse universe = k_EUniverseInvalid;
	if (universe == k_EUniverseInvalid)
	{
		if (Interfaces::GetSteamAPIContext()->SteamUtils())
			universe = Interfaces::GetSteamAPIContext()->SteamUtils()->GetConnectedUniverse();
		else
		{
			PluginWarning("Steam libraries not available - assuming public universe for user Steam IDs!\n");
			universe = k_EUniversePublic;
		}
	}

	std::string gameName;
	switch (player->GetTeam())
	{
		case TFTeam::Red:
			gameName = m_FormatRed->GetString();
			break;

		case TFTeam::Blue:
			gameName = m_FormatBlue->GetString();
			break;

		default:
			gameName = "%alias%";
			break;
	}

	CSteamID playerSteamID(pinfo->friendsID, 1, universe, k_EAccountTypeIndividual);
	FindAndReplaceInString(gameName, "%alias%", GetAlias(playerSteamID, pinfo->name));

	V_strcpy_safe(pinfo->name, gameName.c_str());

	Funcs::GetHook_IVEngineClient_GetPlayerInfo()->SetState(HookAction::SUPERCEDE);
	return result;
}

const std::string& PlayerAliases::GetAlias(const CSteamID& player, const std::string& gameAlias) const
{
	if (!player.IsValid())
		return gameAlias;

	auto found = m_CustomAliases.find(player);
	if (found != m_CustomAliases.end())
		return found->second;

	return gameAlias;
}

void PlayerAliases::FindAndReplaceInString(std::string &str, const std::string &find, const std::string &replace)
{
	if (find.empty())
		return;

	size_t start_pos = 0;

	while ((start_pos = str.find(find, start_pos)) != std::string::npos)
	{
		str.replace(start_pos, find.length(), replace);
		start_pos += replace.length();
	}
}

void PlayerAliases::ToggleEnabled(IConVar* var, const char* oldValue, float fOldValue)
{
	if (m_Enabled->GetBool())
	{
		if (!m_GetPlayerInfoHook)
		{
			m_GetPlayerInfoHook = Funcs::GetHook_IVEngineClient_GetPlayerInfo()->AddHook(std::bind(&PlayerAliases::GetPlayerInfoOverride, this, std::placeholders::_1, std::placeholders::_2));
		}
	}
	else
	{
		if (m_GetPlayerInfoHook && Funcs::GetHook_IVEngineClient_GetPlayerInfo()->RemoveHook(m_GetPlayerInfoHook, __FUNCSIG__))
			m_GetPlayerInfoHook = 0;
	}
}