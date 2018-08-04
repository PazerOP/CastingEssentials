#include "CameraState.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "Misc/HLTVCameraHack.h"
#include "Modules/CameraTools.h"
#include "Modules/CameraSmooths.h"
#include "Modules/FOVOverride.h"

#include <cdll_int.h>
#include <vprof.h>

#undef min
#undef max

MODULE_REGISTER(CameraState);

CameraState::CameraState() :
	m_InToolModeHook(std::bind(&CameraState::InToolModeOverride, this)),
	m_IsThirdPersonCameraHook(std::bind(&CameraState::IsThirdPersonCameraOverride, this)),
	m_SetupEngineViewHook(std::bind(&CameraState::SetupEngineViewOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
{
	InitViews();

	m_LastSpecTarget = 0;
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

bool CameraState::InToolModeOverride()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	m_LastFrameInToolMode = m_ThisFrameInToolMode;

	m_ThisFrameInToolMode = false;

	{
		ICameraOverride* module;

		if ((module = CameraTools::GetModule()) != nullptr)
			m_ThisFrameInToolMode = module->InToolModeOverride();

		if (!m_ThisFrameInToolMode && (module = CameraSmooths::GetModule()) != nullptr)
			m_ThisFrameInToolMode = module->InToolModeOverride();

		if (!m_ThisFrameInToolMode && (module = FOVOverride::GetModule()) != nullptr)
			m_ThisFrameInToolMode = module->InToolModeOverride();
	}

	if (m_ThisFrameInToolMode)
		GetHooks()->SetState<HookFunc::IClientEngineTools_InToolMode>(Hooking::HookAction::SUPERCEDE);

	return m_ThisFrameInToolMode;
}

bool CameraState::IsThirdPersonCameraOverride()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	m_LastFrameIsThirdPerson = m_ThisFrameIsThirdPerson;

	m_ThisFrameIsThirdPerson = false;

	{
		ICameraOverride* module;

		if ((module = CameraTools::GetModule()) != nullptr)
			m_ThisFrameIsThirdPerson = module->IsThirdPersonCameraOverride();

		if (!m_ThisFrameIsThirdPerson && (module = CameraSmooths::GetModule()) != nullptr)
			m_ThisFrameIsThirdPerson = module->IsThirdPersonCameraOverride();

		if (!m_ThisFrameIsThirdPerson && (module = FOVOverride::GetModule()) != nullptr)
			m_ThisFrameIsThirdPerson = module->IsThirdPersonCameraOverride();
	}

	if (m_ThisFrameIsThirdPerson)
		GetHooks()->SetState<HookFunc::IClientEngineTools_IsThirdPersonCamera>(Hooking::HookAction::SUPERCEDE);

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
	{
		ICameraOverride* module;

		if ((module = CameraTools::GetModule()) != nullptr)
		{
			retVal = module->SetupEngineViewOverride(origin, angles, fov);
			m_ThisFramePluginView.Set(origin, angles, fov);
		}

		if ((module = CameraSmooths::GetModule()) != nullptr)
		{
			retVal = module->SetupEngineViewOverride(origin, angles, fov) || retVal;
			m_ThisFramePluginView.Set(origin, angles, fov);
		}

		if ((module = FOVOverride::GetModule()) != nullptr)
		{
			retVal = module->SetupEngineViewOverride(origin, angles, fov) || retVal;
			m_ThisFramePluginView.Set(origin, angles, fov);
		}
	}

	if (retVal)
		GetHooks()->SetState<HookFunc::IClientEngineTools_SetupEngineView>(Hooking::HookAction::SUPERCEDE);

	return retVal;
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

void CameraState::LevelInit()
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
	m_InToolModeHook.SetEnabled(connect);
	m_IsThirdPersonCameraHook.SetEnabled(connect);
	m_SetupEngineViewHook.SetEnabled(connect);
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
