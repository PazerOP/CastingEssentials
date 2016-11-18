#pragma once
#include <Color.h>
#include <dbg.h>
#include <string>
#include <steam/steamclientpublic.h>

#define PLUGIN_VERSION "0.1"

class CCommand;
class ConVar;
class Vector;
class QAngle;

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

inline bool IsStringEmpty(const std::string& s) { return s.empty(); }
inline bool IsStringEmpty(const std::string* s)
{
	if (!s)
		return true;

	return s->empty();
}
inline bool IsStringEmpty(const char* s)
{
	if (!s)
		return true;

	return !(s[0]);
}

inline std::string strprintf(const char* fmt, ...)
{
	va_list v;
	va_start(v, fmt);
	const auto length = vsnprintf(nullptr, 0, fmt, v) + 1;
	va_end(v);

	Assert(length > 0);
	if (length <= 0)
		return nullptr;

	std::string retVal;
	retVal.resize(length);
	va_start(v, fmt);
	const auto written = vsnprintf(&retVal[0], length, fmt, v);
	va_end(v);

	Assert((written + 1) == length);

	return retVal;
}

inline const char* stristr(const char* const searchThis, const char* const forThis)
{
	// smh
	std::string lowerSearchThis(searchThis);
	for (auto& c : lowerSearchThis)
		c = tolower(c);

	std::string lowerForThis(forThis);
	for (auto& c : lowerForThis)
		c = tolower(c);

	auto const ptr = strstr(lowerSearchThis.c_str(), lowerForThis.c_str());
	if (!ptr)
		return nullptr;

	auto const dist = std::distance(lowerSearchThis.c_str(), ptr);

	return (const char*)(searchThis + dist);
}

// Easing functions, see https://www.desmos.com/calculator/5s0csyhvm7 for live demo
inline float EaseOut(float x, float bias = 0.5)
{
	//Assert(x >= 0 && x <= 1);
	//Assert(bias >= 0 && bias <= 1);
	return 1 - std::pow(1 - std::pow(x, bias), 1 / bias);
}
inline float EaseIn(float x, float bias = 0.5)
{
	//Assert(x >= 0 && x <= 1);
	//Assert(bias >= 0 && bias <= 1);
	return std::pow(1 - std::pow(1 - x, bias), 1 / bias);
}
inline float EaseInSlope(float x, float bias = 0.5)
{
	return std::pow(1 - std::pow(1 - x, bias), (1 / bias) - 1) * std::pow(1 - x, bias - 1);
}

extern CSteamID ParseSteamID(const char* input);
extern std::string RenderSteamID(const CSteamID& id);

extern bool ReparseForSteamIDs(const CCommand& in, CCommand& out);

extern void SwapConVars(ConVar& var1, ConVar& var2);

extern bool ParseFloat3(const char* str, float& f1, float& f2, float& f3);
extern bool ParseVector(Vector& v, const char* str);
extern bool ParseAngle(QAngle& a, const char* str);

static constexpr const char* s_ObserverModes[] =
{
	"None",
	"Deathcam",
	"Freezecam",
	"Fixed",
	"In-eye",
	"Third-person chase",
	"Point of interest",
	"Free roaming",
};