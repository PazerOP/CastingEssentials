#pragma once

#include "Modules/Camera/FirstPersonCamera.h"
#include "Modules/Camera/RoamingCamera.h"
#include "Modules/Camera/SimpleCamera.h"
#include "PluginBase/Hook.h"
#include "PluginBase/Modules.h"

#include <convar.h>
#include <mathlib/vector.h>
#include <shared/igamesystem.h>

class C_BaseEntity;
struct CamStateData;
class ICamera;
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

	static ObserverMode GetLocalObserverMode();
	static C_BaseEntity* GetLocalObserverTarget(bool attachedModesOnly = false);

	C_BaseEntity* GetLastSpecTarget() const;

	CameraConstPtr GetActiveCamera() const { return m_ActiveCamera; }
	const CameraPtr& GetActiveCamera() { return m_ActiveCamera; }
	void SetActiveCamera(const CameraPtr& camera); // Instantly snaps the current camera to this.
	void SetActiveCameraSmooth(CameraPtr camera);  // Sets the current camera, letting CameraStateCallbacks add smooths

	bool IsEngineCameraActive() const { return m_ActiveCamera == m_EngineCamera; }
	CameraConstPtr GetEngineCamera() const { return m_EngineCamera; }

	bool IsRoamingCameraActive() const { return m_ActiveCamera == m_RoamingCamera; }
	auto& GetRoamingCamera() { return m_RoamingCamera; }

private:
	ConCommand ce_camerastate_clear_cameras;
	void ClearCameras();

	ConVar ce_camerastate_debug_cameras;

	bool InToolModeOverride();
	bool IsThirdPersonCameraOverride();
	bool SetupEngineViewOverride(Vector& origin, QAngle& angles, float& fov);

	Hook<HookFunc::CInput_CreateMove> m_CreateMoveHook;
	void CreateMoveOverride(CInput* input, int sequenceNumber, float inputSampleFrametime, bool active);

	void OnTick(bool inGame) override;
	void LevelInit() override;
	void LevelShutdown() override;

	static constexpr int UPDATE_POS_TICK_INTERVAL = 20;
	int m_NextPlayerPosUpdateTick;
	Vector m_LastUpdatedServerPos;
	void UpdateServerPosition(const Vector& origin, const QAngle& angles, float fov);

	int m_LastSpecTarget;
	ObserverMode m_LastSpecMode;

	ModeSwitchReason m_SwitchReason = ModeSwitchReason::Unknown;
	VariablePusher<FnCommandCallback_t> m_SpecModeDetour;
	void SpecModeDetour(const CCommand& cmd);

	VariablePusher<FnCommandCallback_t> m_SpecPlayerDetour;
	void SpecPlayerDetour(const CCommand& cmd);

	void SpecStateChanged(ObserverMode mode, C_BaseEntity* primaryTarget);

	void SetSpecMode(ObserverMode mode);

	Hook<HookFunc::IClientEngineTools_InToolMode> m_InToolModeHook;
	Hook<HookFunc::IClientEngineTools_IsThirdPersonCamera> m_IsThirdPersonCameraHook;
	Hook<HookFunc::IClientEngineTools_SetupEngineView> m_SetupEngineViewHook;
	void SetupHooks(bool connect);

	Hook<HookFunc::C_HLTVCamera_SetMode> m_SetModeHook;
	void SetModeOverride(int iMode);
	Hook<HookFunc::C_HLTVCamera_SetPrimaryTarget> m_SetPrimaryTargetHook;
	void SetPrimaryTargetOverride(int nEntity);

	CameraPtr m_ActiveCamera;

	bool m_QueuedCopyEngineData = false;

	const CameraPtr m_EngineCamera;
	const std::shared_ptr<RoamingCamera> m_RoamingCamera;
};
