#pragma once
#include "Misc/Padding.h"

#include <Color.h>
#include <dbg.h>
#include <mathlib/mathlib.h>

#include "PluginBase/VariablePusher.h"

#include <string>
#include <string_view>

using namespace std::string_literals;
using namespace std::string_view_literals;

#pragma warning(disable : 4355)    // 'this': used in base member initializer list
#pragma warning(disable : 4592)    // 'x': symbol will be dynamically initialized (implementation limitation)
#pragma warning(disable : 4533)    // initialization of 'x' is skipped by 'instruction' -- should only be a warning, but is promoted error for some reason?

//#define PLUGIN_VERSION "r7b1"

static constexpr const char* PLUGIN_NAME = "CastingEssentials";
extern const char* const PLUGIN_VERSION_ID;
extern const char* const PLUGIN_FULL_VERSION;

static constexpr float PI_2 = M_PI * 2;

#define XYZW(v) XYZ(v),(v).w

//#define NDEBUG_PER_FRAME_SUPPORT 1
#ifdef NDEBUG_PER_FRAME_SUPPORT
// Oddly specific
static constexpr float NDEBUG_PERSIST_TILL_NEXT_FRAME = -1.738f;
#else
static constexpr float NDEBUG_PERSIST_TILL_NEXT_FRAME = 0; // NDEBUG_PERSIST_TILL_NEXT_SERVER
#endif

#define VPROF_BUDGETGROUP_CE _T("CastingEssentials")

// For passing into strspn or whatever
static constexpr const char* WHITESPACE_CHARS = "\t\n\v\f\r ";

static constexpr const char* s_ObserverModes[] =
{
	"Unspecified",
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
	"Unspecified",
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
class CSteamID;
class KeyValues;
class Player;
class QAngle;
enum class TFTeam;
class Vector;

template<class... Parameters> __forceinline void PluginMsg(const char* fmt, Parameters... param)
{
	ConColorMsg(Color(0, 153, 153, 255), "[%s] ", PLUGIN_NAME);
	Msg(fmt, param...);
}
template<class... Parameters> __forceinline void PluginWarning(const char* fmt, const Parameters&... param)
{
	ConColorMsg(Color(0, 153, 153, 255), "[%s] ", PLUGIN_NAME);
	Warning(fmt, param...);
}
template<class... Parameters> __forceinline void PluginColorMsg(const Color& color, const char* fmt, Parameters... param)
{
	ConColorMsg(Color(0, 153, 153, 255), "[%s] ", PLUGIN_NAME);
	ConColorMsg(color, fmt, param...);
}

__forceinline constexpr const char* FMT_BOOL(bool value) { return value ? "true" : "false"; }

bool TryParseInteger(const char* str, int& out);
bool TryParseFloat(const char* str, float& out);

inline bool IsStringEmpty(const std::string_view& s) { return s.empty(); }
inline bool IsStringEmpty(const std::string* s)
{
	if (!s)
		return true;

	return s->empty();
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

std::string_view::const_iterator stristr(const std::string_view& searchThis, const std::string_view& forThis);
const char* stristr(const char* searchThis, const char* forThis);

__forceinline float Bezier(float t, float x0, float x1, float x2)
{
	return Lerp(t, Lerp(t, x0, x1), Lerp(t, x1, x2));
}

extern std::string RenderSteamID(const CSteamID& id);
void RenderSteamID(const CSteamID& id, char* buf, size_t length);
template<size_t length> void RenderSteamID(const CSteamID& id, char(&buf)[length]) { RenderSteamID(id, buf, length); }

extern bool ReparseForSteamIDs(const CCommand& in, CCommand& out);

extern void SwapConVars(ConVar& var1, ConVar& var2);

extern bool ColorFromConVar(const ConVar& var, Color& out);
extern Color ColorFromConVar(const ConVar& var, bool* success = nullptr);
extern bool ColorFromString(const char* str, Color& out);
extern Color ColorFromString(const char* str, bool* success = nullptr);
extern Vector ColorToVector(const Color& color);

extern bool ParseFloat3(const char* str, float& f1, float& f2, float& f3);
extern bool ParseVector(Vector& v, const char* str);
extern bool ParseAngle(QAngle& a, const char* str);

extern Vector GetViewOrigin();
extern int GetConLine();
extern struct con_nprint_s* GetConLine(con_nprint_s& data);

// Single-frame Con_NPrintf with automatic line selection.
template<typename... Args> void Con_Printf(const char* fmt, const Args&... args)
{
	con_nprint_s info(0, -1);
	engine->Con_NXPrintf(GetConLine(info), fmt, args...);
}
template<typename... Args> void Con_Printf(const Color& color, const char* fmt, const Args&... args)
{
	con_nprint_s info(0, -1);
	info.color[0] = color[0] / 255.0f;
	info.color[1] = color[1] / 255.0f;
	info.color[2] = color[2] / 255.0f;
	engine->Con_NXPrintf(GetConLine(info), fmt, args...);
}

extern std::string KeyValuesDumpAsString(KeyValues* kv, int indentLevel = 0);

Vector ApproachVector(const Vector& from, const Vector& to, float speed);
void AngleDistance(const QAngle& a1, const QAngle& a2, Vector& dists);
QAngle AngleDiff(const QAngle& start, const QAngle& end);
void AngleNormalize(QAngle& angle);

constexpr float Rad2Deg(float radians)
{
	return radians * float(180.0 / 3.14159265358979323846);
}
constexpr float Deg2Rad(float degrees)
{
	return degrees * float(3.14159265358979323846 / 180);
}

inline float UnscaleFOVByWidthRatio(float scaledFov, float ratio)
{
	return Rad2Deg(2 * std::atan(std::tan(Deg2Rad(scaledFov * 0.5f)) / ratio));
}
inline float UnscaleFOVByAspectRatio(float scaledFOV, float aspectRatio)
{
	return UnscaleFOVByWidthRatio(scaledFOV, aspectRatio / (4.0f / 3.0f));
}

template<typename T> constexpr T* FirstNotNull(T* first, T* second)
{
	return first ? first : second;
}
template<typename T> constexpr T* FirstNotNull(T* first, T* second, T* third)
{
	if (first)
		return first;
	if (second)
		return second;

	return third;
}

// Returns a copy of an object.
template<typename T> constexpr T copy(const T& input) { return input; }

// smh why were these omitted
namespace std
{
	inline std::string to_string(const char* str) { return std::string(str); }
}

template<typename T> struct remove_const_ptr_ref           { using type = T;  };
template<typename T> struct remove_const_ptr_ref<const T&> { using type = T&;  };
template<typename T> struct remove_const_ptr_ref<const T*> { using type = T*; };
template<typename T> using remove_const_ptr_ref_t = typename remove_const_ptr_ref<T>::type;

template<typename T> struct add_const_ptr_ref { using type = T; };
template<typename T> struct add_const_ptr_ref<T&> { using type = const T&; };
template<typename T> struct add_const_ptr_ref<T*> { using type = const T*; };
template<typename T> using add_const_ptr_ref_t = typename add_const_ptr_ref<T>::type;

template<class Type, class RetVal, class... Args> using MemberFnPtr_Const = RetVal(Type::*)(Args...) const;
// const TValue(T::*func)(Args...) const
template<typename T, typename TValue, typename... Args>
inline auto call_const(T* pThis, TValue(T::*func)(Args...) const, Args&&... args)
{
	return const_cast<remove_const_ptr_ref_t<TValue>>((const_cast<const T*>(pThis)->*func)(args...));
}
