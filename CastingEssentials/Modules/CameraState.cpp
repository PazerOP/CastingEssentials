#include "CameraState.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"
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

static IClientEntityList* s_ClientEntityList;
static IEngineTool* s_EngineTool;
static HLTVCameraOverride* s_HLTVCamera;
static IVEngineClient* s_EngineClient;

MODULE_REGISTER(CameraState);

class EngineCamera final : public ICamera
{
	void Reset() override {}
	void Update(float dt) override {}
	int GetAttachedEntity() const override { return 0; }
	bool IsCollapsible() const override { return false; }
	const char* GetDebugName() const override { return "EngineCamera"; }
};

static const auto s_DebugOrbitCamera = std::invoke([]()
{
	auto orbit = std::make_shared<OrbitCamera>();
	orbit->m_OrbitPoint.Init();
	orbit->m_Radius = 1000;
	orbit->m_ZOffset = 500;
	return orbit;
});

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
	ce_camerastate_clear_cameras("ce_camerastate_clear_cameras", []() { GetModule()->ClearCameras(); }),

	ce_camerastate_debug_cameras("ce_camerastate_debug_cameras", "0"),

	m_InToolModeHook(std::bind(&CameraState::InToolModeOverride, this)),
	m_IsThirdPersonCameraHook(std::bind(&CameraState::IsThirdPersonCameraOverride, this)),
	m_SetupEngineViewHook(std::bind(&CameraState::SetupEngineViewOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),

	m_SetModeHook(std::bind(&CameraState::SetModeOverride, this, std::placeholders::_1)),
	m_SetPrimaryTargetHook(std::bind(&CameraState::SetPrimaryTargetOverride, this, std::placeholders::_1)),

	m_CreateMoveHook(std::bind(&CameraState::CreateMoveOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)),

	m_EngineCamera(std::make_shared<EngineCamera>()),
	m_RoamingCamera(std::make_shared<RoamingCamera>())
{
	m_LastSpecTarget = 0;
	m_LastSpecMode = OBS_MODE_NONE;

	m_ActiveCamera = m_EngineCamera;
}

bool CameraState::CheckDependencies()
{
	if (!CheckDependency(Interfaces::GetClientEntityList(), s_ClientEntityList))
		return false;
	if (!CheckDependency(Interfaces::GetEngineTool(), s_EngineTool))
		return false;
	if (!CheckDependency(Interfaces::GetHLTVCamera(), s_HLTVCamera))
		return false;
	if (!CheckDependency(Interfaces::GetEngineClient(), s_EngineClient))
		return false;

	return true;
}

ObserverMode CameraState::GetLocalObserverMode()
{
	if (s_EngineClient->IsHLTV())
		return s_HLTVCamera->GetMode();

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

	if (s_EngineClient->IsHLTV())
	{
		return s_HLTVCamera->GetPrimaryTarget();
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

	auto clientEnt = s_ClientEntityList->GetClientEntity(m_LastSpecTarget);
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

void CameraState::ClearCameras()
{
	SetActiveCamera(nullptr);
}

bool CameraState::InToolModeOverride()
{
	if (IsEngineCameraActive() || !GetActiveCamera())
		return false;

	m_InToolModeHook.SetState(Hooking::HookAction::SUPERCEDE);
	return true;
}

bool CameraState::IsThirdPersonCameraOverride()
{
	if (IsEngineCameraActive())
		return false;

	const auto& cam = GetActiveCamera();
	if (!cam || cam->IsFirstPerson())
		return false;

	m_IsThirdPersonCameraHook.SetState(Hooking::HookAction::SUPERCEDE);
	return true;
}

bool CameraState::SetupEngineViewOverride(Vector& origin, QAngle& angles, float& fov)
{
	if (m_QueuedCopyEngineData)
	{
		m_RoamingCamera->SetPosition(origin, angles);
		m_QueuedCopyEngineData = false;
	}

	if (IsEngineCameraActive())
		return false;

	const auto& cam = GetActiveCamera();
	if (!cam)
		return false;

	float dt = s_EngineTool->ClientFrameTime();

	m_ActiveCamera->Update(dt);
	ICamera::TryCollapse(m_ActiveCamera);

	origin = cam->GetOrigin();
	angles = cam->GetAngles();
	fov = cam->GetFOV();

	Assert(origin.IsValid());
	Assert(angles.IsValid());
	Assert(std::isfinite(fov));

	UpdateServerPosition(origin, angles, fov);

	m_SetupEngineViewHook.SetState(Hooking::HookAction::SUPERCEDE);
	return true;
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
		// Update spec mode/target
		{
			auto newMode = GetLocalObserverMode();
			auto newTarget = GetLocalObserverTarget();
			auto newTargetEntindex = newTarget ? newTarget->entindex() : 0;

			if (newMode != m_LastSpecMode || newTargetEntindex != m_LastSpecTarget)
			{
				SpecStateChanged(newMode, newTarget);

				m_LastSpecMode = newMode;
				m_LastSpecTarget = newTargetEntindex;
			}
		}

		if (ce_camerastate_debug_cameras.GetBool())
		{
			auto camera = m_ActiveCamera.get();
			engine->Con_NPrintf(2, "Current camera type: %s", camera ? camera->GetDebugName() : "(null)");
		}
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

	// This only helps on listen servers, but it won't hurt us anywhere else
	if (auto found = cvar->FindVar("sv_force_transmit_ents"))
		found->SetValue(1);
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

		static ConVarRef sv_cheats("sv_cheats");
		if (!sv_cheats.GetBool())
			return;

		const auto tick = s_EngineTool->ClientTick();
		if (tick < m_NextPlayerPosUpdateTick)
			return;

		m_NextPlayerPosUpdateTick = tick + UPDATE_POS_TICK_INTERVAL;

		char buf[128];
		sprintf_s(buf, "spec_goto %f %f %f %f %f %f\n", origin.x, origin.y, origin.z, angles.x, angles.y, angles.z);
		s_EngineTool->Command(buf);
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
				Warning("%s: Failed to \"hook\" %s: command dispatch callback type has changed\n", found->GetName(), GetModuleName());
			}
		}

		if (auto found = cvar->FindCommand("spec_player"))
		{
			auto callback = CCvar::GetDispatchCallback(found);
			if (std::holds_alternative<FnCommandCallback_t*>(callback))
			{
				m_SpecPlayerDetour.Emplace(*std::get<FnCommandCallback_t*>(callback),
					[](const CCommand& cmd) { GetModule()->SpecPlayerDetour(cmd); });
			}
			else
			{
				Warning("%s: Failed to \"hook\" %s: command dispatch callback type has changed\n", found->GetName(), GetModuleName());
			}
		}
	}
	else
		m_SpecModeDetour.Clear();
}

void CameraState::SetModeOverride(int iMode)
{
	// HLTV path
	Assert(!"FIXME");
	//auto mode = (ObserverMode)iMode;
	//SpecModeChanged(mode);
}

void CameraState::SpecPlayerDetour(const CCommand& cmd)
{
	Warning("PARSED: %s\n", cmd.GetCommandString());
	m_SpecPlayerDetour.GetOldValue()(cmd);
}

void CameraState::SpecStateChanged(ObserverMode mode, C_BaseEntity* primaryTarget)
{
	if (auto local = Player::GetLocalPlayer(); local && local->GetTeam() != TFTeam::Spectator)
	{
		SetActiveCamera(m_EngineCamera);
		return;
	}

	CamStateData state;
	state.m_Mode = mode;
	state.m_PrimaryTarget = primaryTarget;

	CameraStateCallbacks::RunSetupCameraState(state);

	CameraPtr newCamera = m_EngineCamera;

	if (state.m_Mode == OBS_MODE_ROAMING)
	{
		if (state.m_PrimaryTarget)
			m_QueuedCopyEngineData = true;

		newCamera = s_DebugOrbitCamera;
		//newCamera = m_RoamingCamera;
	}
	else if (state.m_PrimaryTarget)
	{
		if (state.m_Mode == OBS_MODE_IN_EYE)
			newCamera = std::make_shared<FirstPersonCamera>(state.m_PrimaryTarget);
	}

	// default camera so we know something's broken
	if (!newCamera)
		newCamera = s_DebugOrbitCamera;

	CameraStateCallbacks::RunSetupCameraTarget(state, newCamera);
	CameraStateCallbacks::RunSetupCameraSmooth(state, m_ActiveCamera, newCamera);

	SetActiveCamera(newCamera);
}

void CameraState::SetPrimaryTargetOverride(int nEntity)
{
	Assert(!"FIXME");
	//SpecTargetChanged(nEntity);
}

void CameraState::SpecModeDetour(const CCommand& cmd)
{
	auto pusher = CreateVariablePusher(m_SwitchReason, ModeSwitchReason::SpecMode);
	m_SwitchReason = ModeSwitchReason::SpecMode;

	m_SpecModeDetour.GetOldValue()(cmd);
}