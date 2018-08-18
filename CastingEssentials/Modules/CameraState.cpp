#include "CameraState.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "Misc/HLTVCameraHack.h"
#include "Modules/Camera/ICamera.h"
#include "Modules/CameraTools.h"
#include "Modules/CameraSmooths.h"
#include "Modules/FOVOverride.h"

#include <cdll_int.h>
#include <vprof.h>

#undef min
#undef max

MODULE_REGISTER(CameraState);

CameraState::CameraState() :
	CAutoGameSystemPerFrame("Camera State Per-Frame Camera Updates"),

	m_InToolModeHook(std::bind(&CameraState::InToolModeOverride, this)),
	m_IsThirdPersonCameraHook(std::bind(&CameraState::IsThirdPersonCameraOverride, this)),
	m_SetupEngineViewHook(std::bind(&CameraState::SetupEngineViewOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),

	m_EngineCamera(std::make_shared<SimpleCamera>())
{
	m_LastSpecTarget = 0;

	m_ActiveCamera = m_EngineCamera;
}

ObserverMode CameraState::GetLocalObserverMode()
{
	if (auto engineClient = Interfaces::GetEngineClient(); engineClient && engineClient->IsHLTV())
	{
		if (auto hltvCamera = Interfaces::GetHLTVCamera())
			return hltvCamera->GetMode();
	}

	if (const auto localPlayer = Player::GetLocalPlayer())
		return localPlayer->GetObserverMode();

	Assert(!"Unable to determine current observer mode");
	return OBS_MODE_NONE;
}

C_BaseEntity* CameraState::GetLocalObserverTarget(bool attachedModesOnly)
{
	if (attachedModesOnly)
	{
		if (auto mode = GetLocalObserverMode(); mode != ObserverMode::OBS_MODE_CHASE && mode != ObserverMode::OBS_MODE_IN_EYE)
			return nullptr;
	}

	if (auto client = Interfaces::GetEngineClient(); client && client->IsHLTV())
	{
		if (auto hltvcamera = Interfaces::GetHLTVCamera())
			return hltvcamera->GetPrimaryTarget();
	}
	else
	{
		Player* local = Player::GetLocalPlayer();
		if (local)
			return local->GetObserverTarget();
	}

	return nullptr;
}

C_BaseEntity* CameraState::GetLastSpecTarget() const
{
	if (m_LastSpecTarget < 0 || m_LastSpecTarget > MAX_EDICTS)
		return nullptr;

	auto list = Interfaces::GetClientEntityList();
	if (!list)
		return nullptr;

	auto clientEnt = list->GetClientEntity(m_LastSpecTarget);
	return clientEnt ? clientEnt->GetBaseEntity() : nullptr;
}

void CameraState::Update(float dt)
{
	m_ActiveCamera->Update(dt);
}

bool CameraState::InToolModeOverride()
{
	if (GetActiveCamera())
	{
		m_InToolModeHook.SetState(Hooking::HookAction::SUPERCEDE);
		return true;
	}

	return false;
}

bool CameraState::IsThirdPersonCameraOverride()
{
	if (const auto& cam = GetActiveCamera(); cam && !cam->IsFirstPerson())
	{
		m_IsThirdPersonCameraHook.SetState(Hooking::HookAction::SUPERCEDE);
		return true;
	}

	return false;
}

bool CameraState::SetupEngineViewOverride(Vector& origin, QAngle& angles, float& fov)
{
	m_EngineCamera->m_Origin = origin;
	m_EngineCamera->m_Angles = angles;
	m_EngineCamera->m_FOV = fov;
	m_EngineCamera->m_IsFirstPerson = GetLocalObserverMode() == OBS_MODE_IN_EYE;

	if (const auto& cam = GetActiveCamera())
	{
		m_SetupEngineViewHook.SetState(Hooking::HookAction::SUPERCEDE);

		origin = cam->GetOrigin();
		angles = cam->GetAngles();
		fov = cam->GetFOV();

		return true;
	}

	return false;
}

void CameraState::OnTick(bool inGame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (inGame)
	{
		SetupHooks(true);

		// Update last spec target
		if (auto primaryTarget = Interfaces::GetHLTVCamera()->m_iTraget1; primaryTarget > 0 && primaryTarget < MAX_EDICTS)
			m_LastSpecTarget = primaryTarget;
	}
	else
	{
		SetupHooks(false);
	}
}

void CameraState::SetupHooks(bool connect)
{
	m_InToolModeHook.SetEnabled(connect);
	m_IsThirdPersonCameraHook.SetEnabled(connect);
	m_SetupEngineViewHook.SetEnabled(connect);
}