#pragma once
#include <FastDelegate.h>
#include <functional>
#include <map>

class C_HLTVCamera;
class QAngle;
class ICvar;
class C_BaseEntity;
struct model_t;
class IVEngineClient;
struct player_info_s;

class Funcs final
{
public:
	typedef void(__thiscall *SetCameraAngleFn)(C_HLTVCamera*, QAngle&);
	typedef void(__thiscall *SetModeFn)(C_HLTVCamera*, int);
	typedef void(__thiscall *SetPrimaryTargetFn)(C_HLTVCamera*, int);
	typedef void(__fastcall *SetModeDetourFn)(C_HLTVCamera*, void*, int);
	typedef void(__fastcall *SetPrimaryTargetDetourFn)(C_HLTVCamera*, void*, int);

	static SetCameraAngleFn GetFunc_C_HLTVCamera_SetCameraAngle();
	static SetModeFn GetFunc_C_HLTVCamera_SetMode();
	static SetPrimaryTargetFn GetFunc_C_HLTVCamera_SetPrimaryTarget();

	static int AddHook_ICvar_ConsoleColorPrintf(ICvar* instance, fastdelegate::FastDelegate<void, const Color&, const char*> hook, bool post);
	static int AddHook_ICvar_ConsoleDPrintf(ICvar* instance, fastdelegate::FastDelegate<void, const char*> hook, bool post);
	static int AddHook_ICvar_ConsolePrintf(ICvar* instance, fastdelegate::FastDelegate<void, const char*> hook, bool post);
	static int AddHook_IVEngineClient_GetPlayerInfo(IVEngineClient *instance, fastdelegate::FastDelegate<bool, int, player_info_s *> hook, bool post);

	static int AddHook_C_HLTVCamera_SetMode(std::function<void(C_HLTVCamera *, int &)> hook);
	static int AddHook_C_HLTVCamera_SetPrimaryTarget(std::function<void(C_HLTVCamera *, int &)> hook);

	static void CallFunc_C_HLTVCamera_SetMode(C_HLTVCamera *instance, int iMode);
	static void CallFunc_C_HLTVCamera_SetPrimaryTarget(C_HLTVCamera *instance, int nEntity);
	static bool CallFunc_IVEngineClient_GetPlayerInfo(IVEngineClient *instance, int ent_num, player_info_s *pinfo);

	static void RemoveHook_C_HLTVCamera_SetMode(int hookID);
	static void RemoveHook_C_HLTVCamera_SetPrimaryTarget(int hookID);

	static bool RemoveHook(int hookID, const char* funcName);

	static bool Load();
	static bool Unload();

private:
	Funcs() { }
	~Funcs() { }

	static int m_SetModeLastHookRegistered;
	static std::map<int, std::function<void(C_HLTVCamera *, int &)>> m_SetModeHooks;
	//static int m_SetModelLastHookRegistered;
	//static std::map<int, std::function<void(C_BaseEntity *, const model_t *&)>> m_SetModelHooks;
	static int m_SetPrimaryTargetLastHookRegistered;
	static std::map<int, std::function<void(C_HLTVCamera *, int &)>> m_SetPrimaryTargetHooks;

	static SetModeFn m_SetModeOriginal;
	static SetPrimaryTargetFn m_SetPrimaryTargetOriginal;

	static bool AddDetour(void *target, void *detour, void *&original);

	static bool AddDetour_C_HLTVCamera_SetMode(SetModeDetourFn detour);
	static bool AddDetour_C_HLTVCamera_SetPrimaryTarget(SetPrimaryTargetDetourFn detour);

	static void __fastcall Detour_C_HLTVCamera_SetMode(C_HLTVCamera *, void *, int);
	static void __fastcall Detour_C_HLTVCamera_SetPrimaryTarget(C_HLTVCamera *, void *, int);

	static bool RemoveDetour_C_HLTVCamera_SetMode();
	static bool RemoveDetour_C_HLTVCamera_SetPrimaryTarget();

	static bool RemoveDetour(void *target);

	static SetCameraAngleFn s_SetCameraAngleFn;
	static SetModeFn s_SetModeFn;
	static SetPrimaryTargetFn s_SetPrimaryTargetFn;
};

extern void* SignatureScan(const char* moduleName, const char* signature, const char* mask);

namespace SourceHook
{
	class ISourceHook;
}
extern SourceHook::ISourceHook* g_SHPtr;