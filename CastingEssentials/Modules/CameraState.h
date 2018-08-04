#pragma once

#include "PluginBase/HookManager.h"
#include "PluginBase/Modules.h"

#include <mathlib/vector.h>

class C_BaseEntity;
enum ObserverMode;
class Player;

class CameraState final : public Module<CameraState>
{
public:
	CameraState();
	static constexpr __forceinline const char* GetModuleName() { return "Camera State"; }

	const Vector& GetLastFramePluginViewOrigin() const { return m_LastFramePluginView.m_Origin; }
	const QAngle& GetLastFramePluginViewAngles() const { return m_LastFramePluginView.m_Angles; }

	void GetLastFrameEngineView(Vector& origin, QAngle& angles, float* fov = nullptr) const;
	void GetLastFramePluginView(Vector& origin, QAngle& angles, float* fov = nullptr) const;
	void GetThisFrameEngineView(Vector& origin, QAngle& angles, float* fov = nullptr) const;
	void GetThisFramePluginView(Vector& origin, QAngle& angles, float* fov = nullptr) const;

	static ObserverMode GetLocalObserverMode();
	static C_BaseEntity* GetLocalObserverTarget(bool attachedModesOnly = false);

	C_BaseEntity* GetLastSpecTarget() const;

private:
	bool InToolModeOverride();
	bool IsThirdPersonCameraOverride();
	bool SetupEngineViewOverride(Vector& origin, QAngle& angles, float& fov);

	void OnTick(bool inGame) override;
	void LevelInit() override;

	struct View
	{
		View() { Init(); }
		void Init();
		void Set(const Vector& origin, const QAngle& angles, float fov);

		Vector m_Origin;
		QAngle m_Angles;
		float m_FOV;
	};

	View m_LastFrameEngineView;
	View m_LastFramePluginView;
	View m_ThisFrameEngineView;
	View m_ThisFramePluginView;
	void InitViews();

	bool m_LastFrameInToolMode;
	bool m_ThisFrameInToolMode;

	bool m_LastFrameIsThirdPerson;
	bool m_ThisFrameIsThirdPerson;

	int m_LastSpecTarget;

	void Invalidate(bool& b);

	Hook<HookFunc::IClientEngineTools_InToolMode> m_InToolModeHook;
	Hook<HookFunc::IClientEngineTools_IsThirdPersonCamera> m_IsThirdPersonCameraHook;
	Hook<HookFunc::IClientEngineTools_SetupEngineView> m_SetupEngineViewHook;
	void SetupHooks(bool connect);
};

inline void CameraState::GetLastFrameEngineView(Vector& origin, QAngle& angles, float* fov) const
{
	origin = m_LastFrameEngineView.m_Origin;
	angles = m_LastFrameEngineView.m_Angles;

	if (fov)
		*fov = m_LastFrameEngineView.m_FOV;
}
inline void CameraState::GetLastFramePluginView(Vector& origin, QAngle& angles, float* fov) const
{
	origin = m_LastFramePluginView.m_Origin;
	angles = m_LastFramePluginView.m_Angles;

	if (fov)
		*fov = m_LastFrameEngineView.m_FOV;
}
inline void CameraState::GetThisFrameEngineView(Vector& origin, QAngle& angles, float* fov) const
{
	origin = m_ThisFrameEngineView.m_Origin;
	angles = m_ThisFrameEngineView.m_Angles;

	if (fov)
		*fov = m_ThisFrameEngineView.m_FOV;
}
inline void CameraState::GetThisFramePluginView(Vector& origin, QAngle& angles, float* fov) const
{
	origin = m_ThisFramePluginView.m_Origin;
	angles = m_ThisFramePluginView.m_Angles;

	if (fov)
		*fov = m_ThisFramePluginView.m_FOV;
}