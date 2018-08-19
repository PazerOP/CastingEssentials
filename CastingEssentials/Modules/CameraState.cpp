#include "CameraState.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "Misc/CCvar.h"
#include "Misc/HLTVCameraHack.h"
#include "Modules/Camera/ICamera.h"
#include "Modules/Camera/OrbitCamera.h"
#include "Modules/CameraTools.h"
#include "Modules/CameraSmooths.h"
#include "Modules/FOVOverride.h"

#include <cdll_int.h>
#include <client/input.h>
#include <toolframework/ienginetool.h>
#include <vprof.h>

#undef min
#undef max

MODULE_REGISTER(CameraState);

static ConCommand ce_test_toggle_orbit("ce_test_toggle_orbit", [](const CCommand& cmd)
{
	auto module = CameraState::GetModule();
	if (!module)
	{
		Warning("%s: Unable to get CameraState module\n", cmd[0]);
		return;
	}

	if (module->IsEngineCameraActive())
	{
		auto orbit = std::make_shared<OrbitCamera>();
		orbit->m_Radius = 300;
		orbit->m_ZOffset = 50;
		orbit->m_OrbitPoint = module->GetActiveCamera()->GetOrigin();
		orbit->Update(0); // Apply settings
		module->SetActiveCamera(orbit);
	}
	else
	{
		auto activeCam = module->GetActiveCamera().get();
		auto engineCam = module->GetEngineCamera().get();
		Assert(activeCam != engineCam);
		module->SetActiveCamera(nullptr);
	}
});

CameraState::CameraState() :
	CAutoGameSystemPerFrame("Camera State: Per-Frame Camera Updates"),

	ce_camerastate_clear_cameras("ce_camerastate_clear_cameras", []() { GetModule()->ClearCameras(); }),

	m_InToolModeHook(std::bind(&CameraState::InToolModeOverride, this)),
	m_IsThirdPersonCameraHook(std::bind(&CameraState::IsThirdPersonCameraOverride, this)),
	m_SetupEngineViewHook(std::bind(&CameraState::SetupEngineViewOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),

	m_SetModeHook(std::bind(&CameraState::SetModeOverride, this, std::placeholders::_1)),
	m_SetPrimaryTargetHook(std::bind(&CameraState::SetPrimaryTargetOverride, this, std::placeholders::_1)),

	m_CreateMoveHook(std::bind(&CameraState::CreateMoveOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)),

	m_EngineCamera(std::make_shared<SimpleCamera>()),
	m_RoamingCamera(std::make_shared<RoamingCamera>())
{
	m_LastSpecTarget = 0;
	m_LastSpecMode = OBS_MODE_NONE;

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

void CameraState::SetActiveCamera(const CameraPtr& camera)
{
	if (camera)
		m_ActiveCamera = camera;
	else
		m_ActiveCamera = m_EngineCamera;

	m_ActiveCamera->ApplySettings();
}

void CameraState::Update(float dt)
{
	if (IsInGame())
	{
		if (auto currentMode = GetLocalObserverMode(); currentMode != m_LastSpecMode)
			SpecModeChanged(m_LastSpecMode = currentMode);
	}
}

void CameraState::ClearCameras()
{
	SetActiveCamera(nullptr);
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
		float dt = 0;
		if (auto engineTool = Interfaces::GetEngineTool())
			dt = engineTool->ClientFrameTime();

		m_ActiveCamera->Update(dt);
		ICamera::TryCollapse(m_ActiveCamera);

		m_SetupEngineViewHook.SetState(Hooking::HookAction::SUPERCEDE);

		origin = cam->GetOrigin();
		angles = cam->GetAngles();
		fov = cam->GetFOV();

		UpdateServerPosition(origin, angles, fov);

		return true;
	}

	return false;
}

void CameraState::CreateMoveOverride(CInput* pThis, int sequenceNumber, float inputSampleFrametime, bool active)
{
	m_CreateMoveHook.GetOriginal()(pThis, sequenceNumber, inputSampleFrametime, active);

	auto cmd = pThis->GetUserCmd(sequenceNumber);
	Assert(cmd);
	if (cmd)
		m_RoamingCamera->CreateMove(*cmd);
}

void CameraState::OnTick(bool inGame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (inGame)
	{
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
	SetupHooks(true);
	m_NextPlayerPosUpdateTick = -1;

	if (auto engineTool = Interfaces::GetEngineTool())
	{
		engineTool->Command(
			"ce_consoletools_flags_remove sv_force_transmit_ents FCVAR_CHEAT FCVAR_DEVELOPMENTONLY\n"
			"sv_force_transmit_ents 1\n");
	}
}

void CameraState::LevelShutdown()
{
	SetupHooks(false);
}

// Try our best to at least keep the server player *somewhere* near the client's viewpoint (for PVS)
void CameraState::UpdateServerPosition(const Vector& origin, const QAngle& angles, float fov)
{
	if (!engine)
		return;

	if (engine->IsHLTV())
	{
		auto hltv = Interfaces::GetHLTVCamera();
		if (!hltv)
			return;

		hltv->m_vCamOrigin = origin;
		hltv->m_aCamAngle = angles;
		hltv->m_flFOV = fov;
	}
	else
	{
		if (origin == m_LastUpdatedServerPos)
			return;

		//static ConVarRef sv_cheats("sv_cheats");
		//if (!sv_cheats.GetBool())
		//	return;

		auto engineTool = Interfaces::GetEngineTool();
		if (!engineTool)
			return;

		const auto tick = engineTool->ClientTick();
		if (tick < m_NextPlayerPosUpdateTick)
			return;

		m_NextPlayerPosUpdateTick = tick + UPDATE_POS_TICK_INTERVAL;

		char buf[128];
		sprintf_s(buf, "spec_goto %f %f %f %f %f %f\n", origin.x, origin.y, origin.z, angles.x, angles.y, angles.z);
		engineTool->Command(buf);
	}
}

void CameraState::SetupHooks(bool connect)
{
	m_InToolModeHook.SetEnabled(connect);
	m_IsThirdPersonCameraHook.SetEnabled(connect);
	m_SetupEngineViewHook.SetEnabled(connect);

	m_SetModeHook.SetEnabled(connect);
	m_SetPrimaryTargetHook.SetEnabled(connect);

	m_CreateMoveHook.SetEnabled(connect);

	if (connect)
	{
		// Add our (more) reliable spec_mode "hook"
		if (auto found = cvar->FindCommand("spec_mode"); found && false)
		{
			auto callback = CCvar::GetDispatchCallback(found);
			if (std::holds_alternative<FnCommandCallback_t*>(callback))
			{
				m_SpecModeDetour.Emplace(*std::get<FnCommandCallback_t*>(callback),
					[](const CCommand& cmd) { GetModule()->SpecModeDetour(cmd); });
			}
			else
			{
				Warning("%s: Failed to \"hook\" spec_mode: command dispatch callback type has changed\n", GetModuleName());
			}
		}
	}
	else
		m_SpecModeDetour.Clear();
}

void CameraState::SetModeOverride(int iMode)
{
	// HLTV path
	auto mode = (ObserverMode)iMode;
	SpecModeChanged(mode);
}

void CameraState::SpecModeChanged(ObserverMode mode)
{
	switch (mode)
	{
		case OBS_MODE_ROAMING:
			SetActiveCamera(GetRoamingCamera());
			break;

		default:
			SetActiveCamera(nullptr);
	}
}


void CameraState::SetPrimaryTargetOverride(int nEntity)
{
}

void CameraState::SpecModeDetour(const CCommand& cmd)
{
	auto pusher = CreateVariablePusher(m_SwitchReason, ModeSwitchReason::SpecMode);
	m_SwitchReason = ModeSwitchReason::SpecMode;

	m_SpecModeDetour.GetOldValue()(cmd);
}