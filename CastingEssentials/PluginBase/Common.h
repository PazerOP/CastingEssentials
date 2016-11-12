#pragma once
#include <Color.h>
#include <dbg.h>
#include <string>
#include <steam/steamclientpublic.h>

#define PLUGIN_VERSION "0.1"

class CCommand;

template<class... Parameters> __forceinline void PluginMsg(const char* fmt, Parameters... param)
{
	ConColorMsg(Color(0, 153, 153, 255), "[CastingEssentials] ");
	Msg(fmt, param...);
}
template<class... Parameters> __forceinline void PluginWarning(const char* fmt, Parameters... param)
{
	ConColorMsg(Color(0, 153, 153, 255), "[CastingEssentials] ");
	Warning(fmt, param...);
}
template<class... Parameters> __forceinline void PluginColorMsg(const Color& color, const char* fmt, Parameters... param)
{
	ConColorMsg(Color(0, 153, 153, 255), "[CastingEssentials] ");
	ConColorMsg(color, fmt, param...);
}

inline bool IsInteger(const std::string &s)
{
	if (s.empty() || !isdigit(s[0]) || s[0] == '-' || s[0] == '+')
		return false;

	char *p;
	strtoull(s.c_str(), &p, 10);

	return (*p == 0);
}

inline bool IsFloat(const std::string& s)
{
	if (s.empty())
		return false;

	char* p;
	strtof(s.c_str(), &p);

	return (*p == 0);
}

extern CSteamID ParseSteamID(const char* input);
extern std::string RenderSteamID(const CSteamID& id);

extern bool ReparseForSteamIDs(const CCommand& in, CCommand& out);