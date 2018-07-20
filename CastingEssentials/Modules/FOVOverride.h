#pragma once

#include "PluginBase/ICameraOverride.h"
#include "PluginBase/Modules.h"

#include <convar.h>

enum ObserverMode;
class QAngle;
class Vector;

class FOVOverride : public Module<FOVOverride>, public ICameraOverride
{
public:
	FOVOverride();

	static bool CheckDependencies();

	float GetBaseFOV(ObserverMode mode) const;

private:
	ConVar ce_fovoverride_firstperson;
	ConVar ce_fovoverride_thirdperson;
	ConVar ce_fovoverride_roaming;
	ConVar ce_fovoverride_fixed;

	ConVar ce_fovoverride_test;
	ConVar ce_fovoverride_test_enabled;

	bool InToolModeOverride() const override;
	bool IsThirdPersonCameraOverride() const override { return false; }
	bool SetupEngineViewOverride(Vector &origin, QAngle &angles, float &fov) override;
};