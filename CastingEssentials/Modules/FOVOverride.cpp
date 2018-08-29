#include "fovoverride.h"

#include <client/c_baseentity.h>
#include <client/c_baseplayer.h>
#include <icliententity.h>
#include <shareddefs.h>
#include <vprof.h>

#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"
#include "Misc/HLTVCameraHack.h"
#include "Modules/CameraState.h"
#include "Modules/CameraTools.h"

//static_assert(DEBUG, "Fix this before release, dummy");
//MODULE_REGISTER(FOVOverride);

FOVOverride::FOVOverride() :
	ce_fovoverride_firstperson("ce_fovoverride_firstperson", "90"),
	ce_fovoverride_thirdperson("ce_fovoverride_thirdperson", "90"),
	ce_fovoverride_roaming("ce_fovoverride_roaming", "90"),
	ce_fovoverride_fixed("ce_fovoverride_fixed", "90"),
	ce_fovoverride_test("ce_fovoverride_test", "90", FCVAR_NONE, "FOV override to apply in all cases. Only enabled if ce_fovoverride_all_enabled is nonzero. Designed to help with the placement of autocameras."),
	ce_fovoverride_test_enabled("ce_fovoverride_test_enabled", "0", FCVAR_NONE, "Enables ce_fovoverride_test.")
{
}

bool FOVOverride::CheckDependencies()
{
	bool ready = true;

	if (!Interfaces::GetClientEngineTools())
	{
		PluginWarning("Required interface IClientEngineTools for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Interfaces::GetEngineClient())
	{
		PluginWarning("Required interface IVEngineClient for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Interfaces::GetEngineTool())
	{
		PluginWarning("Required interface IEngineTool for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Player::CheckDependencies())
	{
		PluginWarning("Required player helper class for module %s not available!\n", GetModuleName());
		ready = false;
	}

	return ready;
}

float FOVOverride::GetBaseFOV(ObserverMode mode) const
{
	float fov = 0;
	switch (mode)
	{
		case OBS_MODE_IN_EYE:  fov = ce_fovoverride_firstperson.GetFloat(); break;
		case OBS_MODE_CHASE:   fov = ce_fovoverride_thirdperson.GetFloat(); break;
		case OBS_MODE_FIXED:   fov = ce_fovoverride_fixed.GetFloat(); break;
		case OBS_MODE_ROAMING: fov = ce_fovoverride_roaming.GetFloat(); break;
	}

	if (fov == 0)
	{
		static ConVarRef fov_desired("fov_desired");
		fov = fov_desired.GetFloat();
	}

	return fov;
}

bool FOVOverride::SetupEngineViewOverride(Vector&, QAngle&, float &fov)
{
#if false
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);

	if (ce_fovoverride_test_enabled.GetBool())
	{
		fov = ce_fovoverride_test.GetFloat();
	}
	else
	{
		auto camTools = CameraTools::GetModule();

		switch (CameraState::GetModule()->GetLocalObserverMode())
		{
			case OBS_MODE_IN_EYE:
			{
				auto newFov = ce_fovoverride_firstperson.GetFloat();
				if (newFov == 0)
					return false;

				auto target = Player::AsPlayer(CameraState::GetModule()->GetLocalObserverTarget());
				if (!target || target->CheckCondition(TFCond_Zoomed))
					return false;

				fov = newFov;
				break;
			}
			case OBS_MODE_CHASE:
			{
				auto newFov = ce_fovoverride_thirdperson.GetFloat();
				if (newFov == 0)
					return false;

				fov = newFov;
				break;
			}
			case OBS_MODE_ROAMING:
			{
				if (camTools->GetModeSwitchReason() == ModeSwitchReason::SpecPosition)
					return false;

				auto newFov = ce_fovoverride_roaming.GetFloat();
				if (newFov == 0)
					return false;

				fov = newFov;
				break;
			}

			case OBS_MODE_FIXED:
			{
				if (camTools->GetModeSwitchReason() == ModeSwitchReason::SpecPosition)
					return false;

				auto newFov = ce_fovoverride_fixed.GetFloat();
				if (newFov == 0)
					return false;

				fov = newFov;
				break;
			}

			default:
				return false;
		}
	}
#endif
	//fov = ce_fovoverride_fov.GetFloat();
	return true;
}
