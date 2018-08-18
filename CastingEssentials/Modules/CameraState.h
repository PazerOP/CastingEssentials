#pragma once

#include "Modules/Camera/SimpleCamera.h"
#include "PluginBase/Hook.h"
#include "PluginBase/Modules.h"

#include <mathlib/vector.h>
#include <shared/igamesystem.h>

class C_BaseEntity;
class ICamera;
enum ObserverMode;
class Player;

class CameraState final : public Module<CameraState>, CAutoGameSystemPerFrame
{
public:
	CameraState();
	static constexpr __forceinline const char* GetModuleName() { return "Camera State"; }

	static ObserverMode GetLocalObserverMode();
	static C_BaseEntity* GetLocalObserverTarget(bool attachedModesOnly = false);

	C_BaseEntity* GetLastSpecTarget() const;

	CameraConstPtr GetActiveCamera() const { return m_ActiveCamera; }
	const CameraPtr& GetActiveCamera() { return m_ActiveCamera; }
	void SetActiveCamera(const CameraPtr& camera);

	CameraConstPtr GetEngineCamera() const { return m_EngineCamera; }
	const CameraPtr& GetEngineCamera() { return m_EngineCamera; }

protected:
	void Update(float dt) override;

private:
	bool InToolModeOverride();
	bool IsThirdPersonCameraOverride();
	bool SetupEngineViewOverride(Vector& origin, QAngle& angles, float& fov);

	void OnTick(bool inGame) override;

	int m_LastSpecTarget;

	Hook<HookFunc::IClientEngineTools_InToolMode> m_InToolModeHook;
	Hook<HookFunc::IClientEngineTools_IsThirdPersonCamera> m_IsThirdPersonCameraHook;
	Hook<HookFunc::IClientEngineTools_SetupEngineView> m_SetupEngineViewHook;
	void SetupHooks(bool connect);

	CameraPtr m_ActiveCamera;

	const std::shared_ptr<SimpleCamera> m_EngineCamera;
};