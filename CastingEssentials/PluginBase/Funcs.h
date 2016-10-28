#pragma once
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <type_traits>
//#include "PluginBase/EZHook.h"
#include "PluginBase/GroupHook.h"

class C_HLTVCamera;
class QAngle;
class ICvar;
class C_BaseEntity;
struct model_t;
class IVEngineClient;
struct player_info_s;
typedef player_info_s player_info_t;

namespace PLH
{
	class IHook;
	class VFuncSwap;
}

class Funcs final
{
	enum Func
	{
		ICvar_ConsoleColorPrintf,
		ICvar_ConsoleDPrintf,
		ICvar_ConsolePrintf,

		IVEngineClient_GetPlayerInfo,

		C_HLTVCamera_SetCameraAngle,
		C_HLTVCamera_SetMode,
		C_HLTVCamera_SetPrimaryTarget,

		Count,
	};

	typedef GroupVirtualHook<Func, ICvar_ConsoleColorPrintf, true, ICvar, void, const Color&, const char*> Hook_ICvar_ConsoleColorPrintf;
	typedef GroupVirtualHook<Func, ICvar_ConsoleDPrintf, true, ICvar, void, const char*> Hook_ICvar_ConsoleDPrintf;
	typedef GroupVirtualHook<Func, ICvar_ConsolePrintf, true, ICvar, void, const char*> Hook_ICvar_ConsolePrintf;

	typedef GroupVirtualHook<Func, IVEngineClient_GetPlayerInfo, false, IVEngineClient, bool, int, player_info_t*> Hook_IVEngineClient_GetPlayerInfo;

	typedef GroupHook<Func, C_HLTVCamera_SetCameraAngle, false, C_HLTVCamera, void, const QAngle&> Hook_C_HLTVCamera_SetCameraAngle;
	typedef GroupHook<Func, C_HLTVCamera_SetMode, false, C_HLTVCamera, void, int> Hook_C_HLTVCamera_SetMode;
	typedef GroupHook<Func, C_HLTVCamera_SetPrimaryTarget, false, C_HLTVCamera, void, int> Hook_C_HLTVCamera_SetPrimaryTarget;

	typedef std::function<void(const QAngle&)> Func_C_HLTVCamera_SetCameraAngle;
	typedef std::function<void(int)> Func_C_HLTVCamera_SetMode;
	typedef std::function<void(int)> Func_C_HLTVCamera_SetPrimaryTarget;

	typedef void(__thiscall *RawSetCameraAngleFn)(C_HLTVCamera*, const QAngle&);
	typedef void(__thiscall *RawSetModeFn)(C_HLTVCamera*, int);
	typedef void(__thiscall *RawSetPrimaryTargetFn)(C_HLTVCamera*, int);

	static RawSetCameraAngleFn GetRawFunc_C_HLTVCamera_SetCameraAngle();
	static RawSetModeFn GetRawFunc_C_HLTVCamera_SetMode();
	static RawSetPrimaryTargetFn GetRawFunc_C_HLTVCamera_SetPrimaryTarget();

public:
	static Func_C_HLTVCamera_SetCameraAngle GetFunc_C_HLTVCamera_SetCameraAngle();
	static Func_C_HLTVCamera_SetMode GetFunc_C_HLTVCamera_SetMode();
	static Func_C_HLTVCamera_SetPrimaryTarget GetFunc_C_HLTVCamera_SetPrimaryTarget();

	static bool Load();
	static bool Unload();

	static Hook_ICvar_ConsoleColorPrintf* GetHook_ICvar_ConsoleColorPrintf() { return s_Hook_ICvar_ConsoleColorPrintf.get(); }
	static Hook_ICvar_ConsoleDPrintf* GetHook_ICvar_ConsoleDPrintf() { return s_Hook_ICvar_ConsoleDPrintf.get(); }
	static Hook_ICvar_ConsolePrintf* GetHook_ICvar_ConsolePrintf() { return s_Hook_ICvar_ConsolePrintf.get(); }

	static Hook_IVEngineClient_GetPlayerInfo* GetHook_IVEngineClient_GetPlayerInfo() { return s_Hook_IVEngineClient_GetPlayerInfo.get(); }

	static Hook_C_HLTVCamera_SetCameraAngle* GetHook_C_HLTVCamera_SetCameraAngle() { return s_Hook_C_HLTVCamera_SetCameraAngle.get(); }
	static Hook_C_HLTVCamera_SetMode* GetHook_C_HLTVCamera_SetMode() { return s_Hook_C_HLTVCamera_SetMode.get(); }
	static Hook_C_HLTVCamera_SetPrimaryTarget* GetHook_C_HLTVCamera_SetPrimaryTarget() { return s_Hook_C_HLTVCamera_SetPrimaryTarget.get(); }

private:
	Funcs() { }
	~Funcs() { }

	static std::unique_ptr<Hook_ICvar_ConsoleColorPrintf> s_Hook_ICvar_ConsoleColorPrintf;
	static std::unique_ptr<Hook_ICvar_ConsoleDPrintf> s_Hook_ICvar_ConsoleDPrintf;
	static std::unique_ptr<Hook_ICvar_ConsolePrintf> s_Hook_ICvar_ConsolePrintf;

	static std::unique_ptr<Hook_IVEngineClient_GetPlayerInfo> s_Hook_IVEngineClient_GetPlayerInfo;

	static std::unique_ptr<Hook_C_HLTVCamera_SetCameraAngle> s_Hook_C_HLTVCamera_SetCameraAngle;
	static std::unique_ptr<Hook_C_HLTVCamera_SetMode> s_Hook_C_HLTVCamera_SetMode;
	static std::unique_ptr<Hook_C_HLTVCamera_SetPrimaryTarget> s_Hook_C_HLTVCamera_SetPrimaryTarget;
};

extern void* SignatureScan(const char* moduleName, const char* signature, const char* mask);

namespace SourceHook
{
	class ISourceHook;
}
extern SourceHook::ISourceHook* g_SHPtr;