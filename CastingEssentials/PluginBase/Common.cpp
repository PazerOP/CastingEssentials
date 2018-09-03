#include "Common.h"
#include "Interfaces.h"

#include <characterset.h>
#include <cdll_int.h>
#include <con_nprint.h>
#include <KeyValues.h>
#include <steam/steamclientpublic.h>
#include <toolframework/ienginetool.h>
#include <view_shared.h>
#include <vgui/IScheme.h>
#include <vgui_controls/Controls.h>

#include <algorithm>
#include <cctype>
#include <regex>

std::string_view::const_iterator stristr(const std::string_view& searchThis, const std::string_view& forThis)
{
	// https://stackoverflow.com/a/19839371/871842
	return std::search(searchThis.begin(), searchThis.end(), forThis.begin(), forThis.end(),
		[](char a, char b) { return std::toupper(a) == std::toupper(b); });
}

const char* stristr(const char* searchThis, const char* forThis)
{
	const std::string_view searchThisSV(searchThis);
	if (auto found = stristr(searchThisSV, forThis); found != searchThisSV.end())
		return &*found;

	return nullptr;
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

void SwapConVars(ConVar& var1, ConVar& var2)
{
	const std::string value1(var1.GetString());
	var1.SetValue(var2.GetString());
	var2.SetValue(value1.c_str());
}

bool ColorFromConVar(const ConVar& var, Color& out)
{
	return ColorFromString(var.GetString(), out);
}

Color ColorFromConVar(const ConVar& var, bool* success)
{
	return ColorFromString(var.GetString(), success);
}

bool ColorFromString(const char* str, Color& out)
{
	uint8_t r, g, b, a = 255;
	const auto parsed = sscanf_s(str, "%hhu %hhu %hhu %hhu", &r, &g, &b, &a);

	if (parsed == 3 || parsed == 4)
	{
		out = Color(r, g, b, a);
		return true;
	}
	else
	{
		// Ugh, this is horrible, but there doesn't seem to be a method to determine if a given
		// color actually exists in the client scheme. Instead we grab the same color twice, but
		// with different defaults. If the colors are different, that means it doesn't exist.
		auto scheme = vgui::scheme()->GetIScheme(vgui::scheme()->GetScheme("ClientScheme"));
		auto color = scheme->GetColor(str, Color(254, 1, 254, 255));
		if (color != Color(254, 1, 254, 255) || color == scheme->GetColor(str, Color(0, 255, 0, 255))) {
			out = color;
			return true;
		} else
			return false;
	}
}

Color ColorFromString(const char* str, bool* success)
{
	Color retVal;
	bool succ = ColorFromString(str, retVal);

	if (success)
		*success = succ;

	return retVal;
}

Vector ColorToVector(const Color& color)
{
	return Vector(color.r() / 255.0f, color.g() / 255.0f, color.b() / 255.0f);
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
		return vec3_origin;

	CViewSetup view;
	const bool retVal = clientDLL->GetPlayerView(view);
	Assert(retVal);
	if (!retVal)
		return vec3_origin;

	return view.origin;
}

Vector ApproachVector(const Vector& from, const Vector& to, float speed)
{
	const Vector dir = (to - from).Normalized();

	return from + dir * speed;
}

void AngleDistance(const QAngle& a1, const QAngle& a2, Vector& dists)
{
	dists.x = AngleDistance(a1.x, a2.x);
	dists.y = AngleDistance(a1.y, a2.y);
	dists.z = AngleDistance(a1.z, a2.z);
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

con_nprint_s* GetConLine(con_nprint_s& data)
{
	data.index = GetConLine();
	return &data;
}

std::string KeyValuesDumpAsString(KeyValues* kv, int indentLevel)
{
	class DumpContext : public IKeyValuesDumpContextAsText
	{
	public:
		bool KvWriteText(const char* text) override
		{
			m_String += text;
			return true;
		}

		std::string m_String;
	};

	DumpContext context;
	kv->Dump(&context, indentLevel);
	return context.m_String;
}

bool TryParseInteger(const char* str, int& out)
{
	Assert(str);
	if (!str)
		return false;

	char* strEnd;
	out = std::strtol(str, &strEnd, 0);

	return strEnd != str;
}

bool TryParseFloat(const char* str, float& out)
{
	Assert(str);
	if (!str)
		return false;

	char* strEnd;
	out = std::strtof(str, &strEnd);

	return strEnd != str;
}
