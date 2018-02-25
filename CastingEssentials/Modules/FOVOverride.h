#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>

enum ObserverMode;
class QAngle;
class Vector;

class FOVOverride : public Module<FOVOverride>
{
public:
	FOVOverride();

	static bool CheckDependencies();

	float GetBaseFOV(ObserverMode mode) const;

protected:
	void OnTick(bool inGame) override;

private:
	ConVar ce_fovoverride_firstperson;
	ConVar ce_fovoverride_thirdperson;
	ConVar ce_fovoverride_roaming;
	ConVar ce_fovoverride_fixed;

	ConVar ce_fovoverride_test;
	ConVar ce_fovoverride_test_enabled;

	int m_InToolModeHook;
	int m_SetupEngineViewHook;
	int m_GetDefaultFOVHook;
	bool InToolModeOverride();
	bool SetupEngineViewOverride(Vector &origin, QAngle &angles, float &fov);
};