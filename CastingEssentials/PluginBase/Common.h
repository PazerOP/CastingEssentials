#pragma once
#include <Color.h>
#include <dbg.h>
#include <string>
#include <steam/steamclientpublic.h>

#pragma warning(disable : 4592)		// 'x': symbol will be dynamically initialized (implementation limitation)
#pragma warning(disable : 4533)		// initialization of 'x' is skipped by 'instruction' -- should only be a warning, but is promoted error for some reason?

//#define PLUGIN_VERSION "r7b1"

static constexpr const char* PLUGIN_NAME = "CastingEssentials";
extern const char* const PLUGIN_VERSION_ID;
extern const char* const PLUGIN_FULL_VERSION;

#define VPROF_BUDGETGROUP_CE _T("CastingEssentials")

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

static constexpr const char* s_ShortObserverModes[] =
{
	"None",
	"Deathcam",
	"Freezecam",
	"Fixed",
	"Firstperson",
	"Thirdperson",
	"POI",
	"Roaming",
};

class CCommand;
class ConVar;
class Vector;
class QAngle;

template<class... Parameters> __forceinline void PluginMsg(const char* fmt, Parameters... param)
{
	ConColorMsg(Color(0, 153, 153, 255), "[%s] ", PLUGIN_NAME);
	Msg(fmt, param...);
}
template<class... Parameters> __forceinline void PluginWarning(const char* fmt, Parameters... param)
{
	ConColorMsg(Color(0, 153, 153, 255), "[%s] ", PLUGIN_NAME);
	Warning(fmt, param...);
}
template<class... Parameters> __forceinline void PluginColorMsg(const Color& color, const char* fmt, Parameters... param)
{
	ConColorMsg(Color(0, 153, 153, 255), "[%s] ", PLUGIN_NAME);
	ConColorMsg(color, fmt, param...);
}

inline bool IsInteger(const std::string &s)
{
	if (s.empty() || (!isdigit(s[0]) && s[0] != '-' && s[0] != '+'))
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

inline std::string vstrprintf(const char* fmt, va_list args)
{
	va_list args2;
	va_copy(args2, args);

	const auto length = vsnprintf(nullptr, 0, fmt, args) + 1;

	Assert(length > 0);
	if (length <= 0)
		return nullptr;

	std::string retVal;
	retVal.resize(length);
	const auto written = vsnprintf(&retVal[0], length, fmt, args2);

	Assert((written + 1) == length);

	return retVal;
}
inline std::string strprintf(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	return vstrprintf(fmt, args);
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

// Easing functions, see https://www.desmos.com/calculator/wist7qm16z for live demo
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

inline float smoothstep(float x)
{
	return 3.0f*(x*x) - 2.0f*(x*x*x);
}
inline float smootherstep(float x)
{
	return 6.0f*(x*x*x*x*x) - 15.0f*(x*x*x*x) + 10.0f*(x*x*x);
}

extern std::string RenderSteamID(const CSteamID& id);

extern bool ReparseForSteamIDs(const CCommand& in, CCommand& out);

extern void SwapConVars(ConVar& var1, ConVar& var2);

extern bool ParseFloat3(const char* str, float& f1, float& f2, float& f3);
extern bool ParseVector(Vector& v, const char* str);
extern bool ParseAngle(QAngle& a, const char* str);

extern Vector GetViewOrigin();

void ApproachPosition(const Vector& target, Vector& current, float speed);

template <typename T, std::size_t N> constexpr std::size_t arraysize(T const (&)[N]) noexcept { return N; }

constexpr float Rad2Deg(float radians)
{
	return radians * float(180.0 / 3.14159265358979323846);
}
constexpr float Deg2Rad(float degrees)
{
	return degrees * float(3.14159265358979323846 / 180);
}

template<class T, T value>
class VariablePusher final
{
public:
	VariablePusher() = delete;
	VariablePusher(const VariablePusher<T, value>& other) = delete;
	VariablePusher(T& variable) : m_Variable(&variable)
	{
		m_OldValue = std::move(*m_Variable);
		*m_Variable = value;
	}
	~VariablePusher()
	{
		*m_Variable = std::move(m_OldValue);
	}

private:
	T* const m_Variable;
	T m_OldValue;
};

// smh why were these omitted
namespace std
{
	inline std::string to_string(const char* str) { return std::string(str); }
}