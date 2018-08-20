#pragma once

#include "Modules/Camera/CameraStateCallbacks.h"
#include "Modules/Camera/TPLockCamera.h"
#include "PluginBase/Modules.h"

#include <convar.h>

class CameraTPLock final : public Module<CameraTPLock>, CameraStateCallbacks
{
public:
	CameraTPLock();
	static constexpr __forceinline const char* GetModuleName() { return "Camera TPLock"; }

	static bool CheckDependencies();

private:
	ConVar ce_tplock_enable;
	ConVar ce_tplock_taunt_enable;

	ConVar ce_tplock_default_pos;
	ConVar ce_tplock_default_angle;
	ConVar ce_tplock_default_dps;

	ConVar ce_tplock_taunt_pos;
	ConVar ce_tplock_taunt_angle;
	ConVar ce_tplock_taunt_dps;

	ConVar ce_tplock_bone;

	bool m_IsTaunting;
	void UpdateIsTaunting();

	using TPLockValue = TPLockCamera::TPLockValue;
	static bool ParseTPLockValues(const CCommand& valuesIn, std::array<TPLockValue, 3>& valuesOut);
	static bool ParseTPLockValuesInto(ConVar* cv, const char* oldVal, std::array<TPLockValue, 3>& values);
	static bool ParseTPLockValuesInto(ConVar* cv, const char* oldVal, std::array<float, 3>& values);
	static bool ParseTPLockValuesInto(ConVar* cv, const char* oldVal, Vector& vec);
	void TPLockBoneUpdated(ConVar* cvar);

	struct TPLockRuleset
	{
		Vector m_Pos;
		std::array<TPLockValue, 3> m_Angle;
		std::array<float, 3> m_DPS;
		std::string m_Bone;
	};
	TPLockRuleset m_TPLockDefault;
	TPLockRuleset m_TPLockTaunt;

	void SetupCameraTarget(const CamStateData& state, CameraPtr& newCamera) override;
};