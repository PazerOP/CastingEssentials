#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>

class QAngle;
class Vector;

class FOVOverride : public Module<FOVOverride>
{
public:
	FOVOverride();

	static bool CheckDependencies();

private:
	int inToolModeHook;
	int setupEngineViewHook;
	bool InToolModeOverride();
	bool SetupEngineViewOverride(Vector &origin, QAngle &angles, float &fov);

	ConVar ce_fovoverride_enabled;
	ConVar ce_fovoverride_fov;
	ConVar ce_fovoverride_zoomed;
	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);
};