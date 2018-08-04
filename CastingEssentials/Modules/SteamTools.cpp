#include "SteamTools.h"

#include "PluginBase/Interfaces.h"

#include <steam/steam_api.h>
#include <toolframework/ienginetool.h>

MODULE_REGISTER(SteamTools);

SteamTools::SteamTools() :
	ce_steamtools_rp_legacy("ce_steamtools_rp_legacy", "", FCVAR_NONE, "The rich presence status displayed in the \"View Game Info\" dialog.",
		[](IConVar*, const char*, float) { GetModule()->UpdateRichPresence(); }),

	ce_steamtools_rp_state("ce_steamtools_rp_state", "", FCVAR_NONE,
		"See wiki. Can be empty, or one of MainMenu, Searching[Generic/MatchGroup], Playing[Generic/MatchGroup/Community], or Loading[Generic/MatchGroup/Community].",
		[](IConVar* var, const char* pOldValue, float) { GetModule()->SetRichPresenceState(static_cast<ConVar*>(var), pOldValue); }),

	ce_steamtools_rp_matchgroup("ce_steamtools_rp_matchgroup", "", FCVAR_NONE,
		"See wiki. Can be empty, or one of Competitive6v6, Casual, SpecialEvent, MannUp, or BootCamp.",
		[](IConVar* var, const char* pOldValue, float) { GetModule()->SetMatchGroup(static_cast<ConVar*>(var), pOldValue); }),

	ce_steamtools_rp_currentmap("ce_steamtools_rp_currentmap", "", FCVAR_NONE, "See wiki. Can be used to insert custom text into a rich presence template.",
		[](IConVar* var, const char*, float) { GetModule()->UpdateRichPresence(); }),

	ce_steamtools_rp_debug("ce_steamtools_rp_debug", []() { GetModule()->PrintRichPresence(); }, "Prints the current rich presence state to console.")
{
	static_assert(std::size(RICH_PRESENCE_STATES) == (size_t)RichPresenceState::COUNT);
	static_assert(std::size(MATCH_GROUPS) == (size_t)MatchGroup::COUNT);

	m_LastRPUpdateTime = -1;
}

bool SteamTools::CheckDependencies()
{
	bool ready = true;

	if (!Interfaces::AreSteamLibrariesAvailable())
	{
		PluginWarning("Required Steam libraries for module %s not available!\n", GetModuleName());
		ready = false;
	}
	if (!Interfaces::GetEngineTool())
	{
		PluginWarning("Required interface IEngineTool for module %s not available!\n", GetModuleName());
		ready = false;
	}

	return ready;
}

void SteamTools::OnTick(bool inGame)
{
	if (m_LastRPUpdateTime <= 0 || Interfaces::GetEngineTool()->GetRealTime() - m_LastRPUpdateTime > RP_UPDATE_INTERVAL)
		UpdateRichPresence();
}

void SteamTools::SetRichPresenceState(ConVar* var, const char* oldValue)
{
	const char* newState = var->GetString();
	if (!newState[0])
		return;

	for (const auto& state : RICH_PRESENCE_STATES)
	{
		if (!stricmp(newState, state))
		{
			if (strcmp(newState, state))
				var->SetValue(state);	// Case was wrong
			else
				UpdateRichPresence();

			return;
		}
	}

	PluginWarning("Unknown rich presence state \"%s\". %s\n", newState, var->GetHelpText());
	var->SetValue(oldValue);
}

void SteamTools::SetMatchGroup(ConVar* var, const char* oldValue)
{
	const char* newGroup = var->GetString();
	if (!newGroup[0])
		return;

	for (const auto& group : MATCH_GROUPS)
	{
		if (!stricmp(newGroup, group))
		{
			if (strcmp(newGroup, group))
				var->SetValue(group);	// Case was wrong
			else
				UpdateRichPresence();

			return;
		}
	}

	PluginWarning("Unknown match group \"%s\". %s\n", newGroup, var->GetHelpText());
	var->SetValue(oldValue);
}

void SteamTools::UpdateRichPresence()
{
	auto friends = Interfaces::GetSteamAPIContext()->SteamFriends();

	// State
	if (const char* state = ce_steamtools_rp_state.GetString(); state[0])
	{
		friends->SetRichPresence("state", state);

		char steamDisplayBuf[128];
		sprintf_s(steamDisplayBuf, "#TF_RichPresence_State_%s", state);
		friends->SetRichPresence("steam_display", steamDisplayBuf);
	}

	// Group
	if (const char* group = ce_steamtools_rp_matchgroup.GetString(); group[0])
	{
		//char buf[128];
		//sprintf_s(buf, "#TF_RichPresence_MatchGroup_%s", group);
		friends->SetRichPresence("matchgrouploc", group);
	}

	// Current map/custom text
	if (const char* map = ce_steamtools_rp_currentmap.GetString(); map[0])
		friends->SetRichPresence("currentmap", map);

	// Legacy rich presence
	if (const char* legacy = ce_steamtools_rp_legacy.GetString(); legacy[0])
		friends->SetRichPresence("status", legacy);

	m_LastRPUpdateTime = Interfaces::GetEngineTool()->GetRealTime();
}

void SteamTools::PrintRichPresence()
{
	auto friends = Interfaces::GetSteamAPIContext()->SteamFriends();

	const auto mySteamID = Interfaces::GetSteamAPIContext()->SteamUser()->GetSteamID();

	auto keyCount = friends->GetFriendRichPresenceKeyCount(mySteamID);
	for (int i = 0; i < keyCount; i++)
	{
		auto key = friends->GetFriendRichPresenceKeyByIndex(mySteamID, i);
		auto value = friends->GetFriendRichPresence(mySteamID, key);
		Msg("FRP[%i]: %s = %s\n", i, key, value);
	}
}
