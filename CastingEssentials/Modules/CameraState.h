#pragma once

#include "Modules/Camera/FirstPersonCamera.h"
#include "Modules/Camera/ICameraGroup.h"
#include "Modules/Camera/RoamingCamera.h"
#include "Modules/Camera/SimpleCamera.h"
#include "PluginBase/Hook.h"
#include "PluginBase/Modules.h"

#include <convar.h>
#include <mathlib/vector.h>
#include <shared/igamesystem.h>

#include <stack>

class C_BaseEntity;
class C_BaseViewModel;
struct CamStateData;
class ICamera;
class IClientEntity;
enum ObserverMode;
class Player;

enum class ModeSwitchReason
{
	Unknown,
	SpecMode, // "spec_mode [mode]" in console
	SpecPosition,
};

class CameraState final : public Module<CameraState>
{
public:
	CameraState();
	static constexpr __forceinline const char* GetModuleName() { return "Camera State"; }
	static bool CheckDependencies();

	ObserverMode GetLocalObserverMode() const;
	IClientEntity* GetLocalObserverTarget(bool attachedModesOnly = false) const;

	// Affected only by spec_mode. May not match GetLocalObserverMode().
	auto GetDesiredObserverMode() const { return m_DesiredSpecMode; }

	auto& GetLastSpecTarget() const { return m_LastSpecTarget; }

	CameraConstPtr GetCurrentCamera() const { return m_ActiveCamera; }
	const CameraPtr& GetCurrentCamera() { return m_ActiveCamera; }

	// Instantly snaps the current camera to this. Clears a set camera group if there is one.
	void SetCamera(const CameraPtr& camera);

	// Sets the current camera, letting CameraStateCallbacks add smooths. Clears a set camera group if there is one.
	void SetCameraSmoothed(CameraPtr camera);

	void SetCameraGroup(const CameraGroupPtr& group);
	void SetCameraGroupSmoothed(const CameraGroupPtr& group);

	bool IsRoamingCameraActive() const { return m_ActiveCamera == m_RoamingCamera; }
	auto& GetRoamingCamera() { return m_RoamingCamera; }

private:
	ConCommand ce_camerastate_clear_cameras;
	void ClearCameras() { SetCamera(nullptr); }

	ConVar ce_camerastate_debug;
	ConVar ce_camerastate_debug_cameras;

	void SetupHooks(bool connect);

	bool CalcView(Vector& origin, QAngle& angles, float& fov);
	void UpdateViewmodels();

	Hook<HookFunc::C_TFViewModel_DrawModel> m_DrawViewmodelHook;
	int DrawViewmodelOverride(C_TFViewModel* pThis, int flags);
	int m_LastDrawViewmodelResult = 0;
	int m_DrawViewmodelEntity;

	Hook<HookFunc::C_BaseViewModel_DrawModel> m_DrawBaseViewmodelHook;
	int DrawBaseViewmodelOverride(C_BaseViewModel* pThis, int flags);

	Hook<HookFunc::C_BaseViewModel_InternalDrawModel> m_InternalDrawBaseViewmodelHook;
	int InternalDrawBaseViewmodelHook(C_BaseViewModel* pThis, int flags);

	Hook<HookFunc::C_TFViewModel_OnPostInternalDrawModel> m_OnPostInternalDrawViewmodelHook;
	bool OnPostInternalDrawViewmodelOverride(C_TFViewModel* pThis, ClientModelRenderInfo_t* info);

	Hook<HookFunc::Global_DrawEconEntityAttachedModels> m_DrawEconEntityAttachedModelsHook;
	void DrawEconEntityAttachedModelsOverride(C_BaseAnimating* pBase, C_EconEntity* pModels, ClientModelRenderInfo_t const* info, int mode);

	Hook<HookFunc::C_BaseAnimating_DrawModel> m_DrawBaseAnimatingHook;
	int DrawBaseAnimatingOverride(C_BaseAnimating* pThis, int flags);
	int m_LastDrawBaseAnimatingResult = 0;

	Hook<HookFunc::C_TFPlayer_DrawOverriddenViewmodel> m_PlayerDrawOverriddenViewmodelHook;
	int PlayerDrawOverriddenViewmodelOverride(C_TFPlayer* pThis, C_BaseViewModel* viewmodel, int flags);

	Hook<HookFunc::C_EconEntity_DrawOverriddenViewmodel> m_EconDrawViewmodelHook;
	int EconDrawViewmodelOverride(C_EconEntity* pThis, C_BaseViewModel* viewmodel, int flags);

	Hook<HookFunc::C_EconEntity_IsOverridingViewmodel> m_EconIsOverridingViewmodelHook;
	bool EconIsOverridingViewmodelOverride(C_EconEntity* pThis);

	Hook<HookFunc::C_EconEntity_UpdateAttachmentModels> m_UpdateAttachmentModelsHook;
	bool UpdateAttachmentModelsOverride(C_EconEntity* pThis);

	Hook<HookFunc::C_BaseCombatWeapon_DrawModel> m_DrawWeaponHook;
	int DrawWeaponOverride(C_BaseCombatWeapon* pThis, int flags);
	int m_LastDrawWeaponResult = 0;
	int m_DrawWeaponEntity;

	Hook<HookFunc::C_TFPlayer_CalcView> m_CalcViewPlayerHook;
	void CalcViewPlayerOverride(C_TFPlayer* pThis, Vector& eyeOrigin, QAngle& eyeAngles, float& zNear, float& zFar, float &fov);

	Hook<HookFunc::C_HLTVCamera_CalcView> m_CalcViewHLTVHook;
	void CalcViewHLTVOverride(C_HLTVCamera* pThis, Vector& origin, QAngle& angles, float& fov);

	Hook<HookFunc::CInput_CreateMove> m_CreateMoveHook;
	void CreateMoveOverride(CInput* input, int sequenceNumber, float inputSampleFrametime, bool active);

	void OnTick(bool inGame) override;
	void LevelInit() override;
	void LevelShutdown() override;

	static constexpr int UPDATE_POS_TICK_INTERVAL = 66 / 4;
	int m_NextPlayerPosUpdateTick;
	Vector m_LastUpdatedServerPos;

	static ObserverMode GetEngineObserverMode();
	static IClientEntity* GetEngineObserverTarget(bool attachedModesOnly = false);

	ObserverMode m_LastSpecMode;
	ObserverMode m_DesiredSpecMode;
	CHandle<IClientEntity> m_LastSpecTarget;
	CHandle<IClientEntity> m_DesiredSpecTarget;

	ModeSwitchReason m_SwitchReason = ModeSwitchReason::Unknown;
	VariablePusher<FnCommandCallback_t> m_SpecModeDetour;
	void SpecModeDetour(const CCommand& cmd);

	VariablePusher<FnCommandCallback_t> m_SpecPlayerDetour;
	void SpecPlayerDetour(const CCommand& cmd);

	VariablePusher<FnCommandCallback_t> m_SpecNextDetour;
	void SpecNextDetour(const CCommand& cmd) { SpecNextEntity(false); }
	VariablePusher<FnCommandCallback_t> m_SpecPrevDetour;
	void SpecPrevDetour(const CCommand& cmd) { SpecNextEntity(true); }

	void SpecEntity(int entindex);
	void SpecEntity(IClientEntity* ent);
	void SpecNextEntity(bool backwards);

	void SpecStateChanged(ObserverMode mode, IClientEntity* primaryTarget);

	static ObserverMode ToObserverMode(CameraType type);

	// Instantly snaps the current camera to this. Does not clear the current camera group.
	void SetCameraInternal(const CameraPtr& camera);

	// Sets the current camera, letting CameraStateCallbacks add smooths. Does not clear the current camera group.
	void SetCameraSmoothedInternal(CameraPtr camera);

	CameraPtr m_ActiveCamera;

	CameraPtr m_LastCameraGroupCamera;
	CameraGroupPtr m_CurrentCameraGroup;
	void UpdateFromCameraGroup();

	const CameraPtr m_EngineCamera;
	std::shared_ptr<RoamingCamera> m_RoamingCamera;
};
