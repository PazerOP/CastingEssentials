#pragma once

#include <mathlib/vector.h>

#include "PluginBase/Modules.h"

class ConVar;
class IConVar;

class FOVOverride : public Module
{
public:
	FOVOverride();

	static bool CheckDependencies();

	static FOVOverride* GetModule() { return Modules().GetModule<FOVOverride>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<FOVOverride>().c_str(); }

private:
	int inToolModeHook;
	int setupEngineViewHook;
	bool InToolModeOverride();
	bool SetupEngineViewOverride(Vector &origin, QAngle &angles, float &fov);

	class HLTVCameraOverride;

	ConVar *enabled;
	ConVar *m_FOV;
	ConVar *zoomed;
	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);
};