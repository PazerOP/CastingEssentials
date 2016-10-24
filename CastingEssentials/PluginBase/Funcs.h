#pragma once
#include <FastDelegate.h>

class C_HLTVCamera;
class QAngle;
class ICvar;

typedef void(__thiscall *SetCameraAngleFn)(C_HLTVCamera*, QAngle&);
typedef void(__thiscall *SetModeFn)(C_HLTVCamera*, int);
typedef void(__thiscall *SetPrimaryTargetFn)(C_HLTVCamera*, int);

class Funcs final
{
public:
	static SetCameraAngleFn GetFunc_C_HLTVCamera_SetCameraAngle();
	static SetModeFn GetFunc_C_HLTVCamera_SetMode();
	static SetPrimaryTargetFn GetFunc_C_HLTVCamera_SetPrimaryTarget();

	static int AddHook_ICvar_ConsoleColorPrintf(ICvar* instance, fastdelegate::FastDelegate<void, const Color&, const char*> hook, bool post);
	static int AddHook_ICvar_ConsoleDPrintf(ICvar* instance, fastdelegate::FastDelegate<void, const char*> hook, bool post);
	static int AddHook_ICvar_ConsolePrintf(ICvar* instance, fastdelegate::FastDelegate<void, const char*> hook, bool post);

	static bool RemoveHook(int hookID, const char* funcName);

	static bool Load();
	static bool Unload();

private:
	Funcs() { }
	~Funcs() { }

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