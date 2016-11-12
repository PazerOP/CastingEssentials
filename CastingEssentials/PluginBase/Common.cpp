#include "Common.h"

#include <convar.h>
#include <characterset.h>

#include <regex>

CSteamID ParseSteamID(const char* input)
{
	if (!input)
		return k_steamIDNil;

	std::cmatch match;
	if (std::regex_search(input, match, std::regex("STEAM_[01]:([01]):(\\d+)", std::regex_constants::icase)))
	{
		// Classic SteamID
		return CSteamID(
			strtoul(match[2].first, nullptr, 10) * 2 + strtoul(match[1].first, nullptr, 10),
			k_EUniversePublic,
			k_EAccountTypeIndividual);
	}
	else if (std::regex_search(input, match, std::regex("u:([01]):(\\d+)", std::regex_constants::icase)))
	{
		// SteamID 3
		return CSteamID(
			strtoul(match[2].first, nullptr, 10),
			(EUniverse)strtoul(match[1].first, nullptr, 10),
			k_EAccountTypeIndividual);
	}
	else if (isdigit(input[0]))
	{
		// SteamID64
		return CSteamID(strtoull(input, nullptr, 10));
	}

	return k_steamIDNil;
}

std::string RenderSteamID(const CSteamID& id)
{
	Assert(id.BIndividualAccount());
	if (!id.BIndividualAccount())
		return std::string();

	return std::string("[U:") + std::to_string(id.GetEUniverse()) + ':' + std::to_string(id.GetAccountID()) + ']';
}

bool ReparseForSteamIDs(const CCommand& in, CCommand& out)
{
	characterset_t newSet;
	CharacterSetBuild(&newSet, "{}()'");	// Everything the default set has, minus the ':'
	if (!out.Tokenize(in.GetCommandString(), &newSet))
	{
		Warning("Failed to reparse command string \"%s\"\n", in.GetCommandString());
		return false;
	}

	return true;
}