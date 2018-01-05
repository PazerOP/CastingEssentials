#pragma once

#include "PluginBase/Modules.h"

class ConVar;
class IConVar;
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

	class HLTVCameraOverride;

	ConVar *enabled;
	ConVar *m_FOV;
	ConVar *zoomed;
	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);
};