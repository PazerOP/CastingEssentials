#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>

class SteamTools : public Module<SteamTools>
{
public:
	SteamTools();

	static bool CheckDependencies();

protected:
	void OnTick(bool inGame) override;

private:
	ConVar ce_steamtools_rp_legacy;

	ConVar ce_steamtools_rp_state;
	ConVar ce_steamtools_rp_matchgroup;
	ConVar ce_steamtools_rp_currentmap;

	ConCommand ce_steamtools_rp_debug;

	enum class RichPresenceState
	{
		MainMenu,
		SearchingGeneric,
		SearchingMatchGroup,
		PlayingGeneric,
		LoadingGeneric,
		PlayingMatchGroup,
		LoadingMatchGroup,
		PlayingCommunity,
		LoadingCommunity,

		COUNT,
	};
	static constexpr const char* RICH_PRESENCE_STATES[] =
	{
		"MainMenu",
		"SearchingGeneric",
		"SearchingMatchGroup",
		"PlayingGeneric",
		"LoadingGeneric",
		"PlayingMatchGroup",
		"LoadingMatchGroup",
		"PlayingCommunity",
		"LoadingCommunity",
	};

	enum class MatchGroup
	{
		Competitive6v6,
		Casual,
		SpecialEvent,
		MannUp,
		BootCamp,

		COUNT,
	};
	static constexpr const char* MATCH_GROUPS[] =
	{
		"Competitive6v6",
		"Casual",
		"SpecialEvent",
		"MannUp",
		"BootCamp",
	};

	static constexpr float RP_UPDATE_INTERVAL = 10;

	void SetRichPresenceState(ConVar* var, const char* oldValue);
	void SetMatchGroup(ConVar* var, const char* oldValue);

	void UpdateRichPresence();

	float m_LastRPUpdateTime;

	static void PrintRichPresence();
};