#include "Common.h"
#include "Interfaces.h"

#include <convar.h>
#include <characterset.h>
#include <cdll_int.h>
#include <view_shared.h>
#include <toolframework/ienginetool.h>

#include <regex>

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

void SwapConVars(ConVar& var1, ConVar& var2)
{
	const std::string value1(var1.GetString());
	var1.SetValue(var2.GetString());
	var2.SetValue(value1.c_str());
}

bool ParseFloat3(const char* str, float& f1, float& f2, float& f3)
{
	return sscanf_s(str, "%f %f %f", &f1, &f2, &f3) == 3;
}

bool ParseVector(Vector& v, const char* str)
{
	return ParseFloat3(str, v.x, v.y, v.z);
}

bool ParseAngle(QAngle& a, const char* str)
{
	const auto scanned = sscanf_s(str, "%f %f %f", &a.x, &a.y, &a.z);
	if (scanned == 2)
	{
		a.z = 0;
		return true;
	}
	else if (scanned == 3)
		return true;

	return false;
}

Vector GetViewOrigin()
{
	auto const clientDLL = Interfaces::GetClientDLL();
	Assert(clientDLL);
	if (!clientDLL)
		return Vector();

	CViewSetup view;
	const bool retVal = clientDLL->GetPlayerView(view);
	Assert(retVal);
	if (!retVal)
		return Vector();

	return view.origin;
}

void ApproachPosition(const Vector& target, Vector& current, float speed)
{
	const Vector dir = (target - current).Normalized();

	current = current + dir * speed;
}

int GetConLine()
{
	static int s_LastConLine = 0;
	static int s_LastFrame = 0;
	
	const int thisFrame = Interfaces::GetEngineTool()->HostFrameCount();
	if (thisFrame != s_LastFrame)
	{
		s_LastConLine = 0;
		s_LastFrame = thisFrame;
	}

	return s_LastConLine++;
}
