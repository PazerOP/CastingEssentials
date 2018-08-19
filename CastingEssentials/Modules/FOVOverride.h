#pragma once

#include "Modules/Camera/ICameraStateCallbacks.h"
#include "PluginBase/Modules.h"

#include <convar.h>

enum ObserverMode;
class QAngle;
class Vector;

class FOVOverride : public Module<FOVOverride>, ICameraStateCallbacks
{
public:
	FOVOverride();

	static bool CheckDependencies();
	static constexpr __forceinline const char* GetModuleName() { return "FOV Override"; }

	float GetBaseFOV(ObserverMode mode) const;

private:
	ConVar ce_fovoverride_firstperson;
	ConVar ce_fovoverride_thirdperson;
	ConVar ce_fovoverride_roaming;
	ConVar ce_fovoverride_fixed;

	ConVar ce_fovoverride_test;
	ConVar ce_fovoverride_test_enabled;

	bool SetupEngineViewOverride(Vector &origin, QAngle &angles, float &fov);
};