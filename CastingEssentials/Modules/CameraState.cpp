#include "CameraState.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "Misc/HLTVCameraHack.h"
#include "Modules/CameraTools.h"
#include "Modules/CameraSmooths.h"

#include <cdll_int.h>
#include <vprof.h>

#undef min
#undef max

CameraState::CameraState()
{
	InitViews();

	m_HooksAttached = false;
	m_InToolModeHook = 0;
	m_IsThirdPersonCameraHook = 0;
	m_SetupEngineViewHook = 0;
}

ObserverMode CameraState::GetObserverMode()
{
	if (auto engineClient = Interfaces::GetEngineClient(); engineClient->IsHLTV())
	{
		if (auto hltvCamera = Interfaces::GetHLTVCamera(); hltvCamera)
			return (ObserverMode)hltvCamera->m_nCameraMode;
	}

	if (const auto localPlayer = Player::GetLocalPlayer(); localPlayer)
		return localPlayer->GetObserverMode();

	Assert(!"Unable to determine current observer mode");
	return OBS_MODE_NONE;
}

bool CameraState::InToolModeOverride()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	m_LastFrameInToolMode = m_ThisFrameInToolMode;

	m_ThisFrameInToolMode = false;

	{
		if (CameraTools::GetModule())
			m_ThisFrameInToolMode = static_cast<ICameraOverride*>(CameraTools::GetModule())->InToolModeOverride() || m_ThisFrameInToolMode;

		if (CameraSmooths::GetModule())
			m_ThisFrameInToolMode = static_cast<ICameraOverride*>(CameraSmooths::GetModule())->InToolModeOverride() || m_ThisFrameInToolMode;
	}

	if (m_ThisFrameInToolMode)
		GetHooks()->SetState<IClientEngineTools_InToolMode>(Hooking::HookAction::SUPERCEDE);

	return m_ThisFrameInToolMode;
}

bool CameraState::IsThirdPersonCameraOverride()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	m_LastFrameIsThirdPerson = m_ThisFrameIsThirdPerson;

	m_ThisFrameIsThirdPerson = false;

	{
		if (CameraTools::GetModule())
			m_ThisFrameIsThirdPerson = static_cast<ICameraOverride*>(CameraTools::GetModule())->IsThirdPersonCameraOverride() || m_ThisFrameIsThirdPerson;

		if (CameraSmooths::GetModule())
			m_ThisFrameIsThirdPerson = static_cast<ICameraOverride*>(CameraSmooths::GetModule())->IsThirdPersonCameraOverride() || m_ThisFrameIsThirdPerson;
	}

	if (m_ThisFrameIsThirdPerson)
		GetHooks()->SetState<IClientEngineTools_IsThirdPersonCamera>(Hooking::HookAction::SUPERCEDE);

	return m_ThisFrameIsThirdPerson;
}

bool CameraState::SetupEngineViewOverride(Vector& origin, QAngle& angles, float& fov)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	m_LastFrameEngineView = m_ThisFrameEngineView;
	m_LastFramePluginView = m_ThisFramePluginView;

	m_ThisFrameEngineView.Set(origin, angles, fov);
	m_ThisFramePluginView.Init();

	bool retVal = false;
	if (CameraTools::GetModule())
		retVal = static_cast<ICameraOverride*>(CameraTools::GetModule())->SetupEngineViewOverride(origin, angles, fov) || retVal;

	m_ThisFramePluginView.Set(origin, angles, fov);

	if (CameraSmooths::GetModule())
		retVal = static_cast<ICameraOverride*>(CameraSmooths::GetModule())->SetupEngineViewOverride(origin, angles, fov) || retVal;

	m_ThisFramePluginView.Set(origin, angles, fov);

	if (retVal)
		GetHooks()->SetState<IClientEngineTools_SetupEngineView>(Hooking::HookAction::SUPERCEDE);

	return retVal;
}

void CameraState::OnTick(bool inGame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (inGame)
	{
		if (!m_HooksAttached)
			SetupHooks(m_HooksAttached = true);
	}
	else
	{
		if (m_HooksAttached)
			SetupHooks(m_HooksAttached = false);
	}
}

void CameraState::LevelInitPreEntity()
{
	InitViews();
}

void CameraState::InitViews()
{
	m_LastFrameEngineView.Init();
	m_LastFramePluginView.Init();
	m_ThisFrameEngineView.Init();
	m_ThisFramePluginView.Init();

	Invalidate(m_LastFrameInToolMode);
	Invalidate(m_ThisFrameInToolMode);

	Invalidate(m_LastFrameIsThirdPerson);
	Invalidate(m_ThisFrameIsThirdPerson);
}

void CameraState::Invalidate(bool& b)
{
	static_assert(sizeof(byte) == sizeof(decltype(b)), "Fix this function for a different compiler than VS2015");
	*(byte*)(&b) = std::numeric_limits<byte>::max();
}

void CameraState::SetupHooks(bool connect)
{
	if (connect)
	{
		m_InToolModeHook = GetHooks()->AddHook<IClientEngineTools_InToolMode>(std::bind(&CameraState::InToolModeOverride, this));
		m_IsThirdPersonCameraHook = GetHooks()->AddHook<IClientEngineTools_IsThirdPersonCamera>(std::bind(&CameraState::IsThirdPersonCameraOverride, this));
		m_SetupEngineViewHook = GetHooks()->AddHook<IClientEngineTools_SetupEngineView>(std::bind(&CameraState::SetupEngineViewOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	}
	else
	{
		if (m_InToolModeHook && GetHooks()->RemoveHook<IClientEngineTools_InToolMode>(m_InToolModeHook, __FUNCSIG__))
			m_InToolModeHook = 0;

		Assert(!m_InToolModeHook);

		if (m_IsThirdPersonCameraHook && GetHooks()->RemoveHook<IClientEngineTools_IsThirdPersonCamera>(m_IsThirdPersonCameraHook, __FUNCSIG__))
			m_IsThirdPersonCameraHook = 0;

		Assert(!m_IsThirdPersonCameraHook);

		if (m_SetupEngineViewHook && GetHooks()->RemoveHook<IClientEngineTools_SetupEngineView>(m_SetupEngineViewHook, __FUNCSIG__))
			m_SetupEngineViewHook = 0;

		Assert(!m_SetupEngineViewHook);
	}
}

void CameraState::View::Init()
{
	m_Origin.Invalidate();
	m_Angles.Invalidate();
	m_FOV = VEC_T_NAN;
}

void CameraState::View::Set(const Vector& origin, const QAngle& angles, float fov)
{
	m_Origin = origin;
	m_Angles = angles;
	m_FOV = fov;
}
