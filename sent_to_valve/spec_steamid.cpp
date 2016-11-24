void CSteamID::SetFromString(const char* pchSteamID, EUniverse eDefaultUniverse)
{
	uint64 steam1_ID;

	int steam2_ID1, steam2_ID2, steam2_ID3;

	char steam3AccType;
	int steam3_ID1, steam3_ID2;

	if (sscanf(pchSteamID, "[%c:%i:%i]", &steam3AccType, &steam3_ID1, &steam3_ID2) == 3 ||
		sscanf(pchSteamID, "%c:%i:%i", &steam3AccType, &steam3_ID1, &steam3_ID2) == 3)
	{
		EAccountType accountType;
		if (steam3AccType == 'u' || steam3AccType == 'U')
			accountType = k_EAccountTypeIndividual;
		else if (steam3AccType == 'm' || steam3AccType == 'M')
			accountType = k_EAccountTypeMultiseat;
		else if (steam3AccType == 'G')
			accountType = k_EAccountTypeGameServer;
		else if (steam3AccType == 'A')
			accountType = k_EAccountTypeAnonGameServer;
		else if (steam3AccType == 'p' || steam3AccType == 'P')
			accountType = k_EAccountTypePending;
		else if (steam3AccType == 'C')
			accountType = k_EAccountTypeContentServer;
		else if (steam3AccType == 'g')
			accountType = k_EAccountTypeClan;
		else if (steam3AccType == 't' || steam3AccType == 'T' || steam3AccType == 'l' || steam3AccType == 'L' || steam3AccType == 'c')
			accountType = k_EAccountTypeChat;
		else if (steam3AccType == 'a')
			accountType = k_EAccountTypeAnonUser;
		else
			accountType = k_EAccountTypeInvalid;

		Set(steam3_ID2, (EUniverse)steam3_ID1, accountType);
	}
	else if (sscanf(pchSteamID, "STEAM_%i:%i:%i", &steam2_ID1, &steam2_ID2, &steam2_ID3) == 3)
	{
		Set(steam2_ID3 * 2 + steam2_ID2, (EUniverse)steam2_ID1, k_EAccountTypeIndividual);
	}
	else if (sscanf(pchSteamID, "%llu", &steam1_ID) == 1)
	{
		SetFromUint64(steam1_ID);
	}
	else
	{
		// Failed to parse
		*this = k_steamIDNil;
	}
}

// Required includes
#include "convar.h"
#include "cdll_int.h"
#include "cdll_client_int.h"
#include "hltvcamera.h"
#include "util_shared.h"
#include "c_baseplayer.h"
#include "characterset.h"

CON_COMMAND_F(spec_steamid, "Spectate player with the given SteamID", FCVAR_CLIENTCMD_CAN_EXECUTE)
{
	// Default CCommand::Tokenize breaks up arguments at ':' characters
	CCommand reparsedArgs;
	{
		characterset_t newSet;
		CharacterSetBuild(&newSet, "{}()'");	// Everything the default set has, minus the ':'
		if (!reparsedArgs.Tokenize(args.GetCommandString(), &newSet))
		{
			Warning("spec_steamid: Failed to reparse command string \"%s\"\n", args.GetCommandString());
			return;
		}
	}

	if (reparsedArgs.ArgC() != 2)
	{
		Warning("Usage: spec_steamid <steamID>\n");
		return;
	}

	CSteamID parsedID;
	parsedID.SetFromString(reparsedArgs.Arg(1), k_EUniversePublic);
	if (parsedID == k_steamIDNil)
	{
		Warning("spec_steamid: Failed to parse steam ID \"%s\"\n", reparsedArgs.Arg(1));
		return;
	}

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		CSteamID currentID;
		{
			player_info_t info;
			if (!engine->GetPlayerInfo(i, &info))
				continue;

			currentID.SetFromString(info.guid, k_EUniversePublic);
		}

		if (parsedID == currentID)
		{
			// Switch to this player
			if (engine->IsHLTV())
			{
				if (!HLTVCamera()->IsPVSLocked())
					HLTVCamera()->SetPrimaryTarget(i);
				else
					Warning("spec_steamid: HLTV PVS is currently locked.\n");
			}
			else
			{
				// Re-use spec_player server command so valve only has to paste in 1 function :)
				char buffer[64];
				snprintf(buffer, sizeof(buffer), "spec_player %i", i);

				CCommand specPlayerCmd;
				specPlayerCmd.Tokenize(buffer);
				ForwardSpecCmdToServer(specPlayerCmd);
			}

			return;
		}
	}

	Warning("spec_steamid: No player found on server with steam ID %s\n", reparsedArgs.Arg(1));
}