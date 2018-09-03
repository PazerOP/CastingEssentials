#include "CameraState.h"
#include "PluginBase/Entities.h"
#include "PluginBase/EntityOffsetIterator.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"
#include "Misc/CCvar.h"
#include "Misc/HLTVCameraHack.h"
#include "Modules/Camera/ICamera.h"
#include "Modules/Camera/OrbitCamera.h"
#include "Modules/Camera/PlayerCameraGroup.h"
#include "Modules/CameraTools.h"
#include "Modules/CameraSmooths.h"

#include <cdll_int.h>
#include <client/c_basecombatweapon.h>
#include <client/c_baseplayer.h>
#include <client/c_baseviewmodel.h>
#include <client/input.h>
#include <con_nprint.h>
#include <model_types.h>
#include <toolframework/ienginetool.h>
#include <vprof.h>

#undef min
#undef max

static IClientEntityList* s_ClientEntityList;
static IEngineTool* s_EngineTool;
static HLTVCameraOverride* s_HLTVCamera;
static IVEngineClient* s_EngineClient;

static EntityOffset<ObserverMode> s_PlayerObserverModeOffset;
static EntityOffset<EHANDLE> s_PlayerObserverTargetOffset;
static EntityOffset<CHandle<C_BaseViewModel>[2]> s_PlayerViewModelsOffset;
static EntityOffset<CHandle<C_BaseCombatCharacter>> s_WeaponOwnerOffset;

MODULE_REGISTER(CameraState);

class EngineCamera final : public ICamera
{
public:
	EngineCamera()
	{
		m_Type = CameraType::Roaming;
		m_Origin.Init();
		m_FOV = 90;
		m_Angles.Init();
	}

private:
	void Reset() override {}
	void Update(float dt, uint32_t frame) override {}
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

CameraState::CameraState() :
	ce_camerastate_clear_cameras("ce_camerastate_clear_cameras", []() { GetModule()->ClearCameras(); }),

	ce_camerastate_debug("ce_camerastate_debug", "0"),
	ce_camerastate_debug_cameras("ce_camerastate_debug_cameras", "0"),

	m_CalcViewPlayerHook(std::bind(&CameraState::CalcViewPlayerOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6)),
	m_CalcViewHLTVHook(std::bind(&CameraState::CalcViewHLTVOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)),
	m_CreateMoveHook(std::bind(&CameraState::CreateMoveOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)),
	m_PlayerCreateMoveHook(std::bind(&CameraState::PlayerCreateMoveOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),

	m_EngineCamera(std::make_shared<EngineCamera>())
{
	m_LastSpecTarget = 0;
	m_LastSpecMode = OBS_MODE_NONE;
	m_LastSpecMode = m_DesiredSpecMode = ObserverMode::OBS_MODE_ROAMING;
	m_LastSpecTarget = m_DesiredSpecTarget = 0;

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

	Entities::GetEntityProp(s_PlayerObserverModeOffset, "CBasePlayer", "m_iObserverMode");
	Entities::GetEntityProp(s_PlayerObserverTargetOffset, "CBasePlayer", "m_hObserverTarget");
	Entities::GetEntityProp(s_PlayerViewModelsOffset, "CTFPlayer", "m_hViewModel[0]");
	Entities::GetEntityProp(s_WeaponOwnerOffset, "CBaseCombatWeapon", "m_hOwner");

	return true;
}

ObserverMode CameraState::GetEngineObserverMode()
{
	if (s_EngineClient->IsHLTV())
		return s_HLTVCamera->GetMode();

	if (const auto localPlayer = Player::GetLocalPlayer())
		return localPlayer->GetObserverMode();

	Assert(!"Unable to determine current observer mode");
	return OBS_MODE_NONE;
}
IClientEntity* CameraState::GetEngineObserverTarget(bool attachedModesOnly)
{
	if (attachedModesOnly)
	{
		if (auto mode = GetEngineObserverMode(); mode != ObserverMode::OBS_MODE_CHASE && mode != ObserverMode::OBS_MODE_IN_EYE)
			return nullptr;
	}

	if (s_EngineClient->IsHLTV())
		return s_HLTVCamera->GetPrimaryTarget();
	else if (auto local = Player::GetLocalPlayer())
		return local->GetObserverTarget();

	return nullptr;
}

ObserverMode CameraState::GetLocalObserverMode() const
{
	if (m_ActiveCamera == m_EngineCamera)
	{
		if (s_EngineClient->IsHLTV())
			return s_HLTVCamera->GetMode();

		if (const auto localPlayer = Player::GetLocalPlayer())
			return localPlayer->GetObserverMode();
	}
	else if (m_ActiveCamera)
	{
		return ToObserverMode(m_ActiveCamera->GetCameraType());
	}

	Assert(!"Unable to determine current observer mode");
	return OBS_MODE_NONE;
}

IClientEntity* CameraState::GetLocalObserverTarget(bool attachedModesOnly) const
{
	if (m_ActiveCamera)
	{
		if (attachedModesOnly)
		{
			if (auto mode = GetLocalObserverMode(); mode != ObserverMode::OBS_MODE_CHASE && mode != ObserverMode::OBS_MODE_IN_EYE)
				return nullptr;
		}

		return s_ClientEntityList->GetClientEntity(m_ActiveCamera->GetAttachedEntity());
	}
	else
		return GetEngineObserverTarget();
}

void CameraState::SetDesiredObserverMode(ObserverMode mode)
{
	m_DesiredSpecMode = mode;
}

void CameraState::SetDesiredObserverTarget(IClientEntity* target)
{
	m_DesiredSpecTarget = target;
}

void CameraState::SetCamera(const CameraPtr& camera)
{
	m_LastCameraGroupCamera.reset();
	m_CurrentCameraGroup.reset();
	SetCameraInternal(camera);
}

void CameraState::SetCameraSmoothed(CameraPtr camera, const SmoothSettings& settings)
{
	m_LastCameraGroupCamera.reset();
	m_CurrentCameraGroup.reset();
	SetCameraSmoothedInternal(std::move(camera), settings);
}

void CameraState::SetCameraGroup(const CameraGroupPtr& group)
{
	m_CurrentCameraGroup = group;
	group->GetBestCamera(m_ActiveCamera);
	m_LastCameraGroupCamera = m_ActiveCamera;
}

void CameraState::SetCameraGroupSmoothed(const CameraGroupPtr& group)
{
	m_CurrentCameraGroup = group;

	CameraPtr newCam = m_ActiveCamera;

	group->GetBestCamera(newCam);
	if (newCam != m_ActiveCamera)
		SetCameraSmoothedInternal(newCam, SmoothSettings());

	m_LastCameraGroupCamera = newCam;
}

bool CameraState::CalcView(Vector& origin, QAngle& angles, float& fov)
{
	if (auto localplayer = Player::GetLocalPlayer(); localplayer && localplayer->GetTeam() != TFTeam::Spectator)
		return false;

	UpdateFromCameraGroup();

	Assert(m_ActiveCamera);
	if (!m_ActiveCamera)
		return false;

	auto& callbacks = CameraStateCallbacks::GetCallbacksParent();

	m_ActiveCamera->TryUpdate(s_EngineTool->ClientFrameTime(), s_EngineTool->HostFrameCount());

	if (m_ActiveCamera->IsCollapsible())
	{
		auto prevCamera = m_ActiveCamera;
		auto newCamera = prevCamera;
		if (ICamera::TryCollapse(newCamera))
		{
			callbacks.CameraCollapsed(prevCamera, newCamera);
			SetCameraInternal(newCamera);
		}
	}

	origin = m_ActiveCamera->GetOrigin();
	angles = m_ActiveCamera->GetAngles();
	fov = m_ActiveCamera->GetFOV();

	{
		QAngle temp(angles);
		s_EngineClient->SetViewAngles(temp);
	}

	Assert(origin.IsValid());
	Assert(angles.IsValid());
	Assert(std::isfinite(fov));
	Assert(!AlmostEqual(fov, 0));

	return true;
}

void CameraState::UpdateViewModels()
{
	const auto newMode = GetLocalObserverMode();
	auto newTarget = Player::AsPlayer(GetLocalObserverTarget());
	if (!newTarget)
		return;

	if (auto basePlayer = newTarget->GetBasePlayer())
	{
		basePlayer->CalcViewModelView(m_ActiveCamera->GetOrigin(), m_ActiveCamera->GetAngles());

		if (m_AttachmentModelsLastMode != newMode || m_AttachmentModelsLastPlayer != basePlayer)
		{
			auto& viewmodels = s_PlayerViewModelsOffset.GetValue(basePlayer);
			for (auto& vm : viewmodels)
			{
				auto vmEnt = vm.Get();
				if (!vmEnt)
					continue;

				vmEnt->UpdateVisibility();

				if (auto wep = vmEnt->GetWeapon())
				{
					// Since we don't call SetObserverTarget or whatever and just overwrite the data ourselves,
					// the game doesn't call this for us.
					wep->UpdateAttachmentModels();

					m_AttachmentModelsLastMode = newMode;
					m_AttachmentModelsLastPlayer = basePlayer;
				}
			}
		}
	}
}

class C_TFViewModel : public C_BaseViewModel {};
class C_TFPlayer : public C_BasePlayer {};

void CameraState::CalcViewPlayerOverride(C_TFPlayer* pThis, Vector& origin, QAngle& angles, float& zNear, float& zFar, float& fov)
{
	// If we're here, we're NOT in HLTV.
	if (CalcView(origin, angles, fov))
	{
		s_PlayerObserverModeOffset.GetValue(pThis) = GetLocalObserverMode();

		auto target = GetLocalObserverTarget();
		auto targetEnt = target ? target->GetBaseEntity() : nullptr;
		s_PlayerObserverTargetOffset.GetValue(pThis) = targetEnt;

		UpdateViewModels();

		if (const auto tick = s_EngineTool->ClientTick(); tick >= m_NextPlayerPosUpdateTick)
		{
			m_NextPlayerPosUpdateTick = tick + UPDATE_POS_TICK_INTERVAL;

			char fullCmdBuf[512];
			fullCmdBuf[0] = '\0';

			char buf[128];
			if (auto player = Player::AsPlayer(target))
			{
				sprintf_s(buf, "spec_player \"#%i\"", player->GetUserID());
				strcat_s(fullCmdBuf, buf);
				engine->ServerCmd(buf);

				engine->ServerCmd("spec_player \"4\"");
			}

#if false
			static ConVarRef cl_spec_mode("cl_spec_mode");
			if (auto mode = GetLocalObserverMode(); mode != cl_spec_mode.GetInt())
			{
				sprintf_s(buf, "spec_mode \"%i\"", GetLocalObserverMode());
				strcat_s(fullCmdBuf, buf);
				engine->ServerCmd(buf);
			}

			static ConVarRef sv_cheats("sv_cheats");
			if (sv_cheats.GetBool())
			{
				sprintf_s(buf, "spec_goto \"%f %f %f %f %f %f\";", origin.x, origin.y, origin.z, angles.x, angles.y, angles.z);
				strcat_s(fullCmdBuf, buf);
				engine->ServerCmd(buf);
			}
#endif
		}

		m_CalcViewPlayerHook.SetState(Hooking::HookAction::SUPERCEDE);
	}
}

void CameraState::CalcViewHLTVOverride(C_HLTVCamera* pThis, Vector& origin, QAngle& angles, float& fov)
{
	// If we're here, we ARE in HLTV.
	if (CalcView(origin, angles, fov))
	{
		auto camera = static_cast<HLTVCameraOverride*>(pThis);

		camera->m_vCamOrigin = origin;
		camera->m_aCamAngle = angles;
		camera->m_flFOV = fov;
		camera->m_nCameraMode = (int)GetLocalObserverMode();

		auto target = GetLocalObserverTarget();
		camera->m_iTraget1 = target ? target->entindex() : 0;

		UpdateViewModels();

		m_CalcViewHLTVHook.SetState(Hooking::HookAction::SUPERCEDE);
	}
}

void CameraState::CreateMoveOverride(CInput* pThis, int sequenceNumber, float inputSampleFrametime, bool active)
{
	return;
	m_CreateMoveHook.GetOriginal()(pThis, sequenceNumber, inputSampleFrametime, active);

	auto cmd = pThis->GetUserCmd(sequenceNumber);
	Assert(cmd);
	if (cmd)
	{
		if (auto cam = dynamic_cast<RoamingCamera*>(m_ActiveCamera.get()))
		{
			cam->CreateMove(*cmd);
			cam->SetInputEnabled(true);
		}
	}
}

bool CameraState::PlayerCreateMoveOverride(C_TFPlayer* pThis, float inputSampleTime, CUserCmd* cmd)
{
	Assert(cmd);
	if (!cmd)
		return false;

	auto roaming = dynamic_cast<RoamingCamera*>(m_ActiveCamera.get());
	if (!roaming)
		return false;

	m_PlayerCreateMoveHook.SetState(Hooking::HookAction::SUPERCEDE);
	const bool retVal = m_PlayerCreateMoveHook.GetOriginal()(pThis, inputSampleTime, cmd);

	roaming->CreateMove(*cmd);
	roaming->SetInputEnabled(true);

	return retVal;
}

void CameraState::OnTick(bool inGame)
{
	if (!inGame)
		return;

	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);

	// Update spec mode/target
	if (m_DesiredSpecMode != m_LastSpecMode || m_DesiredSpecTarget != m_LastSpecTarget)
	{
		SpecStateChanged(m_DesiredSpecMode, m_DesiredSpecTarget);
		m_LastSpecMode = m_DesiredSpecMode;
		m_LastSpecTarget = m_DesiredSpecTarget;
	}

	if (ce_camerastate_debug.GetBool())
	{
		auto currentMode = GetLocalObserverMode();
		engine->Con_NPrintf(GetConLine(), "Current observer mode: %i (%s)", currentMode, s_ShortObserverModes[currentMode]);

		auto currentTarget = GetLocalObserverTarget();
		engine->Con_NPrintf(GetConLine(), "Current observer target: %i (%s)", currentTarget ? currentTarget->entindex() : 0,
			Player::AsPlayer(currentTarget) ? Player::AsPlayer(currentTarget)->GetName() : "none");

		auto engineMode = GetEngineObserverMode();
		engine->Con_NPrintf(GetConLine(), "Engine observer mode: %i (%s)", engineMode, s_ShortObserverModes[engineMode]);

		auto engineTarget = GetEngineObserverTarget();
		engine->Con_NPrintf(GetConLine(), "Engine observer target: %i (%s)", engineTarget ? engineTarget->entindex() : 0,
			Player::AsPlayer(engineTarget) ? Player::AsPlayer(engineTarget)->GetName() : "none");

		auto desiredMode = GetDesiredObserverMode();
		Con_Printf("DESIRED spec mode: %i (%s)", desiredMode, s_ShortObserverModes[desiredMode]);

		auto desiredTarget = GetDesiredObserverTarget();
		Con_Printf("DESIRED spec target: %i (%s)", desiredTarget ? desiredTarget->entindex() : 0,
			Player::AsPlayer(desiredTarget) ? Player::AsPlayer(desiredTarget)->GetName() : "none");
	}

	if (ce_camerastate_debug_cameras.GetBool())
	{
		auto camera = m_ActiveCamera.get();
		engine->Con_NPrintf(GetConLine(), "Current camera type: %s", camera ? camera->GetDebugName() : "(null)");
	}
}

void CameraState::LevelInit()
{
	SetupHooks(true);
	m_NextPlayerPosUpdateTick = -1;

	// This only helps on listen servers, but it won't hurt us anywhere else
	if (auto found = cvar->FindVar("sv_force_transmit_ents"))
		found->SetValue(1);

	auto roaming = std::make_shared<RoamingCamera>();
	roaming->SetPosition(m_ActiveCamera->GetOrigin(), m_ActiveCamera->GetAngles());
	SetCamera(roaming);
}

void CameraState::LevelShutdown()
{
	SetupHooks(false);
	SetCamera(m_EngineCamera);
}

void CameraState::SetupHooks(bool connect)
{
	m_CalcViewHLTVHook.SetEnabled(connect);
	m_CalcViewPlayerHook.SetEnabled(connect);
	m_CreateMoveHook.SetEnabled(connect);
	m_PlayerCreateMoveHook.SetEnabled(connect);

	if (connect)
	{
		const auto OverrideCommand = [](const char* cmd, VariablePusher<FnCommandCallback_t>& pusher, FnCommandCallback_t newCallback)
		{
			if (auto found = cvar->FindCommand(cmd))
			{
				auto callback = CCvar::GetDispatchCallback(found);
				if (std::holds_alternative<FnCommandCallback_t*>(callback))
					pusher.Emplace(*std::get<FnCommandCallback_t*>(callback), newCallback);
				else
					Warning("%s: Failed to \"hook\" %s: command dispatch callback type has changed\n", GetModuleName(), found->GetName());
			}
			else
				Warning("%s: Failed to \"hook\" %s: could not find command\n", GetModuleName(), cmd);
		};

		// Add our (more) reliable "hooks"
		OverrideCommand("spec_mode", m_SpecModeDetour, [](const CCommand& cmd) { GetModule()->SpecModeDetour(cmd); });
		OverrideCommand("spec_player", m_SpecPlayerDetour, [](const CCommand& cmd) { GetModule()->SpecPlayerDetour(cmd); });
		OverrideCommand("spec_next", m_SpecNextDetour, [](const CCommand& cmd) { GetModule()->SpecNextDetour(cmd); });
		OverrideCommand("spec_prev", m_SpecPrevDetour, [](const CCommand& cmd) { GetModule()->SpecPrevDetour(cmd); });
	}
	else
	{
		m_SpecModeDetour.Clear();
		m_SpecPlayerDetour.Clear();
		m_SpecNextDetour.Clear();
		m_SpecPrevDetour.Clear();
	}
}

void CameraState::SpecPlayerDetour(const CCommand& cmd)
{
	if (cmd.ArgC() < 2)
		return Warning("Usage: %s <name/#userid>\n", cmd[0]);

	const bool isNumber = cmd[1][0] == '#';

	int parsed;
	if (isNumber && TryParseInteger(cmd[1] + 1, parsed))
		return SpecUserID(parsed);
	else
	{
		for (auto player : Player::Iterable())
		{
			if (stristr(player->GetName(), cmd[1]))
				return SpecEntity(player->GetEntity());
		}

		Warning("%s: Unable to find player with name containing \"%s\"\n", cmd[0], cmd[1]);
	}
}

void CameraState::SpecEntity(int entindex)
{
	SpecEntity(s_ClientEntityList->GetClientEntity(entindex));
}

void CameraState::SpecUserID(int userid)
{
	if (auto player = Player::GetPlayerFromUserID(userid))
		SpecEntity(player->GetEntity());
	else
		Warning("%s: No player found with userid %i\n", GetModuleName());
}

void CameraState::SpecEntity(IClientEntity* ent)
{
	CameraStateCallbacks::GetCallbacksParent().SpecTargetChanged(m_DesiredSpecTarget.Get(), ent);
	m_DesiredSpecTarget = ent;
}

void CameraState::SpecNextEntity(bool backwards)
{
	// Based on logic from C_HLTVCamera::SpecNextPlayer
	int start = 1;

	const auto maxClients = s_EngineTool->GetMaxClients();

	if (m_DesiredSpecTarget.GetEntryIndex() > 0 && m_DesiredSpecTarget.GetEntryIndex() <= maxClients)
		start = m_DesiredSpecTarget.GetEntryIndex();

	int index = start;

	while (true)
	{
		// got next/prev player
		if (backwards)
			index--;
		else
			index++;

		// check bounds
		if (index < 1)
			index = maxClients;
		else if (index > maxClients)
			index = 1;

		if (index == start)
			return; // couldn't find a new valid player

		auto player = Player::GetPlayer(index);
		if (!player)
			continue;

		// Must be on a real team
		if (auto team = player->GetTeam(); team != TFTeam::Red && team != TFTeam::Blue)
			continue;

		// Ignore dead players
		if (!player->IsAlive())
			continue;

		SpecEntity(index);
		break;
	}
}

void CameraState::SpecStateChanged(ObserverMode mode, IClientEntity* primaryTarget)
{
	if (auto local = Player::GetLocalPlayer(); local && local->GetTeam() != TFTeam::Spectator)
	{
		SetCamera(m_EngineCamera);
		return;
	}

	auto& callbacksParent = CameraStateCallbacks::GetCallbacksParent();

	if (mode == OBS_MODE_ROAMING)
	{
		// Create a new roaming camera so we have keep momentum of our old camera, and have no momentum on our new.
		auto newCam = std::make_shared<RoamingCamera>();

		if (primaryTarget)
			newCam->GotoEntity(primaryTarget);

		CameraPtr newCamAdjusted = newCam;
		callbacksParent.SetupCameraTarget(mode, primaryTarget, newCamAdjusted);

		SetCameraSmoothed(newCamAdjusted);
	}
	else if (mode == OBS_MODE_IN_EYE || mode == OBS_MODE_CHASE)
	{
		if (auto playerCamGroup = dynamic_cast<PlayerCameraGroup*>(m_CurrentCameraGroup.get()))
		{
			if (playerCamGroup->GetPlayerEnt() == primaryTarget)
				return; // Let the camera group handle this itself
		}

		if (auto player = Player::AsPlayer(primaryTarget))
		{
			SetCameraGroupSmoothed(std::make_shared<PlayerCameraGroup>(*Player::AsPlayer(primaryTarget)));
			return;
		}
	}
}

ObserverMode CameraState::ToObserverMode(CameraType type)
{
	switch (type)
	{
		case CameraType::FirstPerson:    return ObserverMode::OBS_MODE_IN_EYE;
		case CameraType::ThirdPerson:    return ObserverMode::OBS_MODE_CHASE;
		case CameraType::Roaming:        return ObserverMode::OBS_MODE_ROAMING;
		case CameraType::Fixed:          return ObserverMode::OBS_MODE_FIXED;

		case CameraType::Smooth:         return ObserverMode::OBS_MODE_FIXED;
	}

	Assert(!"Unknown camera type");
	return ObserverMode::OBS_MODE_NONE;
}

void CameraState::SetCameraInternal(const CameraPtr& camera)
{
	if (auto roaming = dynamic_cast<RoamingCamera*>(m_ActiveCamera.get()))
		roaming->SetInputEnabled(false);

	if (camera)
	{
		m_ActiveCamera = camera;
		m_ActiveCamera->ApplySettings();
	}
	else
		m_ActiveCamera = m_EngineCamera;
}

void CameraState::SetCameraSmoothedInternal(CameraPtr camera, const SmoothSettings& settings)
{
	if (camera != m_ActiveCamera)
	{
		camera->ApplySettings();
		CameraStateCallbacks::GetCallbacksParent().SetupCameraSmooth(m_ActiveCamera, camera, settings);
	}

	SetCameraInternal(camera);
}

void CameraState::UpdateFromCameraGroup()
{
	if (!m_CurrentCameraGroup)
		return;

	CameraPtr nextCam = m_LastCameraGroupCamera ? m_LastCameraGroupCamera : m_ActiveCamera;
	m_CurrentCameraGroup->GetBestCamera(nextCam);

	if (nextCam != m_LastCameraGroupCamera)
	{
		SetCameraSmoothedInternal(nextCam, SmoothSettings());
		m_LastCameraGroupCamera = std::move(nextCam);
	}
}

void CameraState::SpecModeDetour(const CCommand& cmd)
{
	auto newMode = ObserverMode(int(m_DesiredSpecMode) + 1);
	if (cmd.ArgC() > 1)
		newMode = ObserverMode(atoi(cmd[1]));

	switch (newMode)
	{
		default:
		case ObserverMode::OBS_MODE_NONE:
		case ObserverMode::OBS_MODE_DEATHCAM:
		case ObserverMode::OBS_MODE_FREEZECAM:
		case ObserverMode::OBS_MODE_FIXED:
		case ObserverMode::OBS_MODE_IN_EYE:
			newMode = ObserverMode::OBS_MODE_IN_EYE;
			break;

		case ObserverMode::OBS_MODE_CHASE:
			newMode = ObserverMode::OBS_MODE_CHASE;
			break;

		case ObserverMode::OBS_MODE_POI:
		case ObserverMode::OBS_MODE_ROAMING:
			newMode = ObserverMode::OBS_MODE_ROAMING;
			break;
	}

	if (newMode != m_DesiredSpecMode)
		CameraStateCallbacks::GetCallbacksParent().SpecModeChanged(m_DesiredSpecMode, newMode);

	m_DesiredSpecMode = newMode;
}
