#pragma once

#include "Modules/Camera/FirstPersonCamera.h"
#include "Modules/Camera/ICameraGroup.h"
#include "Modules/Camera/RoamingCamera.h"
#include "Modules/Camera/SimpleCamera.h"
#include "Modules/Camera/SmoothSettings.h"
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

	// Affected only by spec_mode. May or may not match GetLocalObserverMode(). If any changes
	// are made to spec_mode or through SetDesiredObserverMode, they will apply next frame.
	void SetDesiredObserverMode(ObserverMode mode);
	ObserverMode GetDesiredObserverMode() const { return m_DesiredSpecMode; }

	void SetDesiredObserverTarget(IClientEntity* target);
	IClientEntity* GetDesiredObserverTarget() const { return m_DesiredSpecTarget.Get(); }

	auto& GetLastSpecTarget() const { return m_LastSpecTarget; }

	CameraConstPtr GetCurrentCamera() const { return m_ActiveCamera; }
	const CameraPtr& GetCurrentCamera() { return m_ActiveCamera; }

	// Instantly snaps the current camera to this. Clears a set camera group if there is one.
	void SetCamera(const CameraPtr& camera);

	// Sets the current camera, letting CameraStateCallbacks add smooths. Clears a set camera group if there is one.
	void SetCameraSmoothed(CameraPtr camera, const SmoothSettings& settings = SmoothSettings());

	void SetCameraGroup(const CameraGroupPtr& group);
	void SetCameraGroupSmoothed(const CameraGroupPtr& group);

private:
	ConCommand ce_camerastate_clear_cameras;
	void ClearCameras() { SetCamera(nullptr); }

	ConVar ce_camerastate_debug;
	ConVar ce_camerastate_debug_cameras;

	void SetupHooks(bool connect);

	bool CalcView(Vector& origin, QAngle& angles, float& fov);

	void UpdateViewModels();
	ObserverMode m_AttachmentModelsLastMode;
	IHandleEntity* m_AttachmentModelsLastPlayer;

	Hook<HookFunc::C_TFPlayer_CalcView> m_CalcViewPlayerHook;
	void CalcViewPlayerOverride(C_TFPlayer* pThis, Vector& eyeOrigin, QAngle& eyeAngles, float& zNear, float& zFar, float &fov);

	Hook<HookFunc::C_HLTVCamera_CalcView> m_CalcViewHLTVHook;
	void CalcViewHLTVOverride(C_HLTVCamera* pThis, Vector& origin, QAngle& angles, float& fov);

	Hook<HookFunc::CInput_CreateMove> m_CreateMoveHook;
	void CreateMoveOverride(CInput* input, int sequenceNumber, float inputSampleFrametime, bool active);

	Hook<HookFunc::C_TFPlayer_CreateMove> m_PlayerCreateMoveHook;
	bool PlayerCreateMoveOverride(C_TFPlayer* pThis, float inputSampleTime, CUserCmd* cmd);

	Hook<HookFunc::SVC_FixAngle_ReadFromBuffer> m_FixAngleHook;
	bool FixAngleOverride(SVC_FixAngle* pThis, bf_read& buffer);

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
	void SpecUserID(int userid);
	void SpecEntity(IClientEntity* ent);
	void SpecNextEntity(bool backwards);

	void SpecStateChanged(ObserverMode mode, IClientEntity* primaryTarget);

	static ObserverMode ToObserverMode(CameraType type);

	// Instantly snaps the current camera to this. Does not clear the current camera group.
	void SetCameraInternal(const CameraPtr& camera);

	// Sets the current camera, letting CameraStateCallbacks add smooths. Does not clear the current camera group.
	void SetCameraSmoothedInternal(CameraPtr camera, const SmoothSettings& settings);

	CameraPtr m_ActiveCamera;

	CameraPtr m_LastCameraGroupCamera;
	CameraGroupPtr m_CurrentCameraGroup;
	void UpdateFromCameraGroup();

	const CameraPtr m_EngineCamera;
};
