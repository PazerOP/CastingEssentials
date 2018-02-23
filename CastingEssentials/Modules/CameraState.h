#pragma once

#include "PluginBase/Modules.h"

#include <mathlib/vector.h>

enum ObserverMode;

class CameraState final : public Module<CameraState>
{
public:
	CameraState();

	const Vector& GetLastFramePluginViewOrigin() const { return m_LastFramePluginView.m_Origin; }
	const QAngle& GetLastFramePluginViewAngles() const { return m_LastFramePluginView.m_Angles; }

	void GetLastFrameEngineView(Vector& origin, QAngle& angles, float* fov = nullptr) const;
	void GetLastFramePluginView(Vector& origin, QAngle& angles, float* fov = nullptr) const;
	void GetThisFrameEngineView(Vector& origin, QAngle& angles, float* fov = nullptr) const;
	void GetThisFramePluginView(Vector& origin, QAngle& angles, float* fov = nullptr) const;

	ObserverMode GetObserverMode() const;

private:
	bool InToolModeOverride();
	bool IsThirdPersonCameraOverride();
	bool SetupEngineViewOverride(Vector& origin, QAngle& angles, float& fov);

	void OnTick(bool inGame) override;
	void LevelInitPreEntity() override;

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

	void Invalidate(bool& b);

	bool m_HooksAttached;
	int m_InToolModeHook;
	int m_IsThirdPersonCameraHook;
	int m_SetupEngineViewHook;
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